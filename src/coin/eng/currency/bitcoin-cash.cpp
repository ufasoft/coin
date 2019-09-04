/*######   Copyright (c)      2019 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com ####
#                                                                                                                                     #
# 		See LICENSE for licensing information                                                                                         #
#####################################################################################################################################*/

#include <el/ext.h>
#include <crypto/cryp/secp256k1.h>

#include "../eng.h"
#include "../script.h"

namespace Coin {

static const int BCASH_EDA_INTERVAL = 144;
static const int MAX_BLOCK_SIGOPS_PER_MB = 20000;

static thread_local bool
	t_bSchnorrMoltisig, t_bCheckDataSigSigOps;

static StackValue MinimallyEncode(const StackValue& v) {
	StackValue r = v;
	if (!r.empty()) {
		uint8_t last = r[r.size() - 1];
		if (!(last & 0x7F)) {
			if (r.size() == 1)
				r.resize(0);
			else if (!(r[r.size() - 2] & 0x80)) {
				for (size_t i = r.size() - 1; i > 0; i--) {
					if (r[i - 1] != 0) {
						if (r[i - 1] & 0x80)
							r.data()[i++] = last;
						 else
							r.data()[i - 1] |= last;
						r.resize(i);
						return r;
					}
				}
				r.resize(0);
			}
		}
	}
	return r;
}

class COIN_CLASS BitcoinCashEng : public CoinEng {
	typedef CoinEng base;

	int m_heightUAHF,
		m_heightEDA,
		m_heightMagneticAnomaly,
		m_heightGraviton = INT_MAX;		//!!!TODO Change in Dec 2019
public:
	BitcoinCashEng(CoinDb& cdb)
		: base(cdb)
	{	
		HrpSeparator = ':';
		MaxOpcode = Opcode::OP_CHECKDATASIGVERIFY;
	}
protected:
	Target GetNextTargetRequired(const BlockHeader& headerLast, const Block& block) override {
		int height = headerLast.Height;
		int nIntervalForMod = GetIntervalForModDivision(height);
		if (height >= m_heightEDA) {
			if (ChainParams.IsTestNet) {
				if (block.get_Timestamp() - headerLast.Timestamp > ChainParams.BlockSpan * 2) // negative block timestamp delta is allowed
					return ChainParams.MaxTarget;
			}

			BlockHeader blockBegin = GetHeaderWithMedianTimestamp(Tree.GetAncestor(headerLast->Hash(), height - 144)),
				blockEnd = GetHeaderWithMedianTimestamp(headerLast);
			TimeSpan span = clamp(blockEnd.Timestamp - blockBegin.Timestamp, TimeSpan::FromHours(12), TimeSpan::FromHours(48));
			int heightBegin = blockBegin.Height;
			static const BigInteger bi2_256 = BigInteger(1) << 256;
			BigInteger work = 0;
			for (BlockHeader b = blockEnd; b.Height != heightBegin; b = Tree.GetHeader(b.PrevBlockHash))
				work += bi2_256 / b->DifficultyTarget;
			return Target(bi2_256 / (work * (int)ChainParams.BlockSpan.count() / (int)duration_cast<seconds>(span).count()) - 1);
		}

		if ((height + 1) % nIntervalForMod == 0)
			return base::GetNextTargetRequired(headerLast, block);

		if (headerLast.GetMedianTimePast() - Tree.GetAncestor(headerLast->Hash(), height - 6).GetMedianTimePast() < TimeSpan::FromHours(12))
			return headerLast->DifficultyTarget;
		BigInteger bi = headerLast->DifficultyTarget;
		return Target(bi + (bi >> 2));
	}

	void SetPolicyFlags(int height) override {
		base::SetPolicyFlags(height);
		if (height < m_heightMagneticAnomaly)
			return;
		t_bCheckDataSigSigOps = true;
		if (height < m_heightGraviton)
			return;
		t_scriptPolicy.MinimalData = true;
		t_bSchnorrMoltisig = true;
	}

	bool IsCheckSigOp(Opcode opcode) {
		return t_bCheckDataSigSigOps && (opcode == Opcode::OP_CHECKDATASIG || opcode == Opcode::OP_CHECKDATASIGVERIFY)
			|| base::IsCheckSigOp(opcode);
	}

	int GetMaxSigOps(const Block& block) override {
		return WITNESS_SCALE_FACTOR * MAX_BLOCK_SIGOPS_PER_MB * ((block.GetSerializeSize() - 1) / 1000000 + 1);
	}

	void OpCat(VmStack& stack) override {
		Blob v = Span(stack[1]) + stack[0];
		stack[1] = v;
		if (v.size() > MAX_SCRIPT_ELEMENT_SIZE)
			Throw(CoinErr::SCRIPT_ERR_PUSH_SIZE);
		stack.SkipStack(1);
	}

	void OpSubStr(VmStack& stack) override {	// OP_SPLIT
		StackValue data = stack[1];
		uint64_t position = explicit_cast<int>(VmStack::ToBigInteger(stack[0]));	//!!!TODO require minimal
		if (position > data.size())
			Throw(CoinErr::SCRIPT_RANGE);
		stack[1] = Span(data).subspan(0, position);
		stack[0] = Span(data).subspan(position);
	}

	void OpDiv(VmStack& stack) override {
		stack[1] = VmStack::FromBigInteger(VmStack::ToBigInteger(stack[1]) / VmStack::ToBigInteger(stack[0]));
		stack.SkipStack(1);
	}

	void OpMod(VmStack& stack) override {
		stack[1] = VmStack::FromBigInteger(VmStack::ToBigInteger(stack[1]) % VmStack::ToBigInteger(stack[0]));
		stack.SkipStack(1);
	}

	void OpAnd(VmStack& stack) override {
		StackValue& vr = stack[1], &v = stack[0];
		if (vr.size() != v.size())
			Throw(CoinErr::SCRIPT_INVALID_ARG);
		uint8_t* pr = vr.data();
		const uint8_t* p = v.data();
		for (int i = 0, n = vr.size(); i < n; ++i)
			pr[i] &= p[i];
		stack.SkipStack(1);
	}

	void OpOr(VmStack& stack) override {
		StackValue& vr = stack[1], &v = stack[0];
		if (vr.size() != v.size())
			Throw(CoinErr::SCRIPT_INVALID_ARG);
		uint8_t* pr = vr.data();
		const uint8_t* p = v.data();
		for (int i = 0, n = vr.size(); i < n; ++i)
			pr[i] |= p[i];
		stack.SkipStack(1);
	}

	void OpXor(VmStack& stack) override {
		StackValue& vr = stack[1], &v = stack[0];
		if (vr.size() != v.size())
			Throw(CoinErr::SCRIPT_INVALID_ARG);
		uint8_t* pr = vr.data();
		const uint8_t* p = v.data();
		for (int i = 0, n = vr.size(); i < n; ++i)
			pr[i] ^= p[i];
		stack.SkipStack(1);
	}

	void OpLeft(VmStack& stack) override {			// OP_NUM2BIN
		StackValue valSize = stack.Pop();
		uint64_t size = explicit_cast<int>(VmStack::ToBigInteger(valSize));	//!!!TODO require minimal
		if (size > MAX_SCRIPT_ELEMENT_SIZE)
			Throw(CoinErr::SCRIPT_ERR_PUSH_SIZE);
		StackValue v = VmStack::FromBigInteger(VmStack::ToBigInteger(stack[0]));
		if (v.size() != size) {
			if (v.size() > size)
				Throw(CoinErr::SCRIPT_ERR_PUSH_SIZE); //!!! error code
			uint8_t signbit = 0x00;
			if (!v.empty()) {
				signbit = v[v.size() - 1] & 0x80;
				v.data()[v.size() - 1] &= 0x7f;
			}
			while (v.size() + 1 < size)
				v.push_back(0);
			v.push_back(signbit);
		}
		stack[0] = v;
	}

	void OpRight(VmStack& stack) override {			// OP_BIN2NUM
		stack[0] = MinimallyEncode(stack[0]);
	}

	bool IsValidSignatureEncoding(RCSpan sig) override {
		return sig.size() == 64 || base::IsValidSignatureEncoding(sig);
	}

	bool VerifyHash(RCSpan pubKey, const HashValue& hash, RCSpan sig) override {
		if (sig.size() != 64)
			return base::VerifyHash(pubKey, hash, sig);
		SchnorrDsa dsa;
		dsa.ParsePubKey(pubKey);
		return dsa.VerifyHash(hash.ToSpan(), sig);
	}

	void OpCheckDataSig(VmStack& stack) override {
		StackValue& sig = stack[2]
			, &msg = stack[1]
			, &pubKey = stack[0];		
		bool bOk = false;
		if (!sig.empty()) {
			hashval hash = SHA256().ComputeHash(msg);
			bOk = VerifyHash(pubKey, hash, sig);
		}
		stack.SkipStack(2);
		stack[0] = bOk ? VmStack::TrueValue : VmStack::FalseValue;
	}

	void OpCheckDataSigVerify(VmStack& stack) override {
		OpCheckDataSig(stack);
		if (stack.Pop().empty())
			Throw(CoinErr::SCRIPT_ERR_CHECKSIGVERIFY);
	}

	void PatchSigHasher(SignatureHasher& sigHasher) override {
		base::PatchSigHasher(sigHasher);
		if (sigHasher.m_txoTo.Height >= m_heightUAHF)
			sigHasher.CalcWitnessCache();
	}

	void CheckBlock(const Block& block) override {
		base::CheckBlock(block);
		if (block.Height >= m_heightMagneticAnomaly) {
			if (!is_sorted(++block.get_Txes().begin(), block.get_Txes().end(), [](const Tx& a, const Tx& b) { return Hash(a) <= Hash(b); }))	// '<=' to prevent duplicates
				Throw(CoinErr::TxOrdering);
		}
	}

	void OrderTxes(CTxes& txes) override {
		sort(txes.begin() + 1, txes.end(), [](const Tx& a, const Tx& b) { return Hash(a) < Hash(b); });
	}

	vector<shared_future<TxFeeTuple>> ConnectBlockTxes(CConnectJob& job, const vector<Tx>& txes, int height) override {
		if (height < m_heightMagneticAnomaly)
			return base::ConnectBlockTxes(job, txes, height);

		bool bVerifySignature = BestBlockHeight() > ChainParams.LastCheckpointHeight - INITIAL_BLOCK_THRESHOLD;
		vector<shared_future<TxFeeTuple>> futsTx;
		futsTx.reserve(txes.size());
		for (auto& tx : txes)
			if (!tx->IsCoinBase() || ChainParams.CoinbaseMaturity == 0)
				job.TxoMap.AddAllOuts(Hash(tx), tx);
		for (auto& tx : txes)
			if (!tx->IsCoinBase())
				ConnectTx(job, futsTx, tx, height, bVerifySignature);
		return futsTx;
	}

	void CheckHashType(SignatureHasher& sigHasher, uint32_t sigHashType) override {
		base::CheckHashType(sigHasher, sigHashType);
		if (sigHasher.m_txoTo.Height >= m_heightUAHF) {
			if (!(sigHashType & (uint32_t)SigHashType::SIGHASH_FORKID))
				Throw(CoinErr::BadBlockSignature);
			uint32_t forkId = sigHashType >> 8;
			if (forkId != 0)
				Throw(CoinErr::BadBlockSignature);
			sigHasher.m_bWitness = true;
		}
	}
private:
	void SortPair(BlockHeader& a, BlockHeader& b) {
		if (a.Timestamp > b.Timestamp)
			std::swap(a, b);
	}

	BlockHeader GetHeaderWithMedianTimestamp(const BlockHeader& h) {
		BlockHeader h1 = Tree.FindHeader(h.PrevBlockHash);
		BlockHeader ar[3] = { Tree.FindHeader(h1.PrevBlockHash), h1, h };
		SortPair(ar[0], ar[2]);		// Don't use sort or stable_sort
		SortPair(ar[0], ar[1]);		// they give different result in following case:
		SortPair(ar[1], ar[2]);		// [2, 1(a), 1(b)]. We must return 1(a)
		return ar[1];
	}

	void SetChainParams(const Coin::ChainParams& p) override {
		base::SetChainParams(p);
		ChainParams.MaxBlockWeight = 32000000;
		if (ChainParams.IsTestNet) {
			m_heightUAHF = 1155876;
			m_heightEDA = 1188697;
			m_heightMagneticAnomaly = 1267997;
		} else {
			m_heightUAHF = 478559;
			m_heightEDA = 504031;
			m_heightMagneticAnomaly = 556767;
		}
	}
};


static CurrencyFactory<BitcoinCashEng> s_bitcoin("Bitcoin Cash"), s_bitcoinTestnet("Bitcoin Cash-testnet");

} // Coin::

