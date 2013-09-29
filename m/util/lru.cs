/*######     Copyright (c) 1997-2012 Ufasoft  http://ufasoft.com  mailto:support@ufasoft.com,  Sergey Pavlov  mailto:dev@ufasoft.com #####
# This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published #
# by the Free Software Foundation; either version 3, or (at your option) any later version. This program is distributed in the hope that #
# it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. #
# See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with this #
# program; If not, see <http://www.gnu.org/licenses/>                                                                                    #
########################################################################################################################################*/

using System;
using System.Collections.Generic;
using System.Diagnostics;


namespace Utils {


public struct LruItem<K, V> {
	internal LinkedListNode<K> m_node;
	internal V m_value;
}

public class LruDictionary<K, V> : Dictionary<K, LruItem<K, V> > {

	int MaxSize = 255;
	LinkedList<K> m_list = new LinkedList<K>();

	public LruDictionary(int maxSize) {
		MaxSize = maxSize;
	}

	void UpNode(LinkedListNode<K> node) {
		m_list.Remove(node);
		m_list.AddFirst(node);
	}

	public void Add(K k, V v) {
		if (ContainsKey(k))
			throw new ArgumentException();

		if (Count >= MaxSize) {
			var last = m_list.Last;

			if (Ut.TraceLevel > 2)
				Debug.Print("RemovingFromLru: {0}", last.Value);

			base.Remove(last.Value);
			m_list.Remove(last);
		}
		var item = new LruItem<K, V>() {m_node = m_list.AddFirst(k), m_value = v};
		base.Add(k, item);
	}

	public new V this[K k] {
		get {
			var item = base[k];
			UpNode(item.m_node);
			return item.m_value;
		}
		set {
			if (ContainsKey(k)) {
				var node = base[k].m_node;
				var newItem = new LruItem<K, V>() { m_node = node, m_value = value };
				base[k] = newItem;
				UpNode(node);
			}
			else
				Add(k, value);
		}
	}

	public bool TryGetValue(K k, out V v) {
		LruItem<K, V> bv;
		bool r = base.TryGetValue(k, out bv);
		v = bv.m_value;
		if (r)
			UpNode(bv.m_node);
		return r;
	}

	public new bool Remove(K k) {
		LruItem<K, V> bv;
		if (base.TryGetValue(k, out bv)) {
			m_list.Remove(bv.m_node);
			return base.Remove(k);
		}
		return false;
	}

}

}

