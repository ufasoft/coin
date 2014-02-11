/*######     Copyright (c) 1997-2013 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #######################################
#                                                                                                                                                                          #
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation;  #
# either version 3, or (at your option) any later version. This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the      #
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details. You should have received a copy of the GNU #
# General Public License along with this program; If not, see <http://www.gnu.org/licenses/>                                                                               #
##########################################################################################################################################################################*/

#include <el/ext.h>

#if UCFG_SPECIAL_CRT
#	include "mutex"
#	include "ostream"
#	include "thread"
#endif

namespace ExtSTL  {
using namespace Ext;

#if !UCFG_CPP11_HAVE_THREAD || (defined(_MSC_VER) && _MSC_VER < 1700) || !UCFG_STDSTL || UCFG_SPECIAL_CRT

unsigned int thread::hardware_concurrency() {
	return Environment.ProcessorCoreCount;
}

ostream& AFXAPI operator<<(ostream& os, const thread::id& v) {
	return os << v.m_tid;
}


namespace this_thread {
	
thread::id AFXAPI get_id() {
	thread::id r;
#if UCFG_USE_PTHREADS
	r.m_tid = ::pthread_self();
#elif UCFG_WDM
	r.m_tid = KeGetCurrentThread();
#else
	r.m_tid = ::GetCurrentThreadId();
#endif
	return r;
}

} // this_thread::

thread::~thread() {
	if (joinable())
		terminate();
}

thread::thread(EXT_RV_REF(thread) rv)
	:	m_t(rv.m_t)
{
	rv.m_t = nullptr;
}

thread& thread::operator=(EXT_RV_REF(thread) rv) {
	m_t = rv.m_t;
	rv.m_t = nullptr;
	return *this;
}

void thread::swap(thread& other) {
	std::swap(m_t, other.m_t);
}

void thread::detach() {
	if (!joinable())
		Ext::ThrowImp(E_INVALIDARG);
	m_t = nullptr;
}

void thread::Start(Thread *t) {
	(m_t = t)->Start();
}

void thread::join() {
	m_t->Join();
	m_t = nullptr;
}

#endif // !UCFG_CPP11_HAVE_THREAD



}  // ExtSTL::


