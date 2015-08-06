#pragma once

#include <cstdint>
#include <unordered_map>
#include <queue>

template<typename T>
class ObjPool
{
public:
	typedef typename std::unordered_map<uint32_t, T*> ObjMap_t;
	typedef typename ObjMap_t::iterator iterator;
	typedef void PostAllocProc(ObjPool<T>*, T*);

	ObjPool() :m_objectId(0), m_PostAlloc(NULL),
		m_usedSize(0), m_initSize(0)
	{
	}

	bool init(
		uint32_t initSize = 0,
		PostAllocProc *postAlloc = NULL)
	{
		m_initSize = initSize;
		m_PostAlloc = postAlloc;

		if (m_initSize <= 0)
			return false;

		for (uint32_t i = 0; i < m_initSize; ++i)
		{
			T* pObj = new T();
			m_objMap[++m_objectId] = pObj;
			if (m_PostAlloc) m_PostAlloc(this, pObj);
			m_freeObjs.push(m_objectId);
		}

		return true;
	}

	inline iterator begin() { return m_objMap.begin(); }
	inline iterator end() { return m_objMap.end(); }
	inline size_t size() { return m_objMap.size(); }
	inline size_t init_size() { return m_initSize; }
	inline size_t max_size() { return m_objMap.max_size(); }
	inline size_t used_size() { return m_usedSize; }
	inline uint32_t GetCurrentId() { return m_objectId; }

	T* alloc(uint32_t* key = NULL)
	{
		if (!m_freeObjs.empty())
		{
			uint32_t objId = m_objectId.front();
			T* pObj = find(objId);
			if (pObj)
			{
				if (key) *key = objId;
				m_freeObjs.pop();
				m_usedSize++;
				return new(pObj)T();
			}
		}

		T* pObj = new T();
		m_objMap[++m_objectId] = pObj;
		if (m_PostAlloc) m_PostAlloc(this, pObj);
		if (key) *key = m_objectId;
		m_usedSize++;
		return pObj;
	}

	bool dealloc(uint32_t key)
	{
		T* pObj = find(key);
		if (!pObj)
			return false;

		if (m_objMap.erase(key) <= 0)
			return false;

		if (m_freeObjs.size() <= m_initSize)
		{
			uint32_t new_key = ++m_objectId;
			m_objMap[new_key] = pObj;
			m_freeObjs.push(new_key);
			if (m_PostAlloc) m_PostAlloc(this, pObj);
		}
		else
		{
			delete pObj;
		}
		m_usedSize--;

		return false;
	}

	T* find(uint32_t key)
	{
		typename ObjMap_t::const_iterator itr
			= m_objMap.find(key);

		return itr != m_objMap.end()
			? itr->second
			: NULL;
	}

private:
	uint32_t m_objectId;
	ObjMap_t m_objMap;
	size_t m_initSize;
	size_t m_usedSize;
	std::queue<uint32_t> m_freeObjs;
	PostAllocProc *m_PostAlloc;
};