#ifndef __CIRCLE_QUEUE_H__
#define __CIRCLE_QUEUE_H__

namespace lce
{

	template <class ElemType>

	class CCircleQueue
	{

	private:
		struct SNode
		{
			ElemType e;
		};

	public:
		CCircleQueue()
		{
			pData = NULL;
			m_dwFront = 0;
			m_dwRear = 0;
			m_dwSize = 0;
		}

		~CCircleQueue()
		{
			if (pData != NULL)
				delete[] pData;
		}

		void init(uint32_t dwSize = 1000)
		{
			if (dwSize < 1)
				dwSize = 1000;

			pData = new SNode[dwSize + 1];
			m_dwFront = 0;
			m_dwRear = 0;
			m_dwSize = dwSize + 1;
		}

		inline bool empty()
		{
			return m_dwFront == m_dwRear;
		}

		inline bool full()
		{
			return ((m_dwRear + 1) % m_dwSize) == m_dwFront;
		}

		bool enque(const ElemType &e)
		{

			if (full())
				return false;

			pData[m_dwRear].e = e;
			m_dwRear = (m_dwRear + 1) % m_dwSize;

			return true;
		}

		bool deque(ElemType &e)
		{

			if (empty())
				return false;

			e = pData[m_dwFront].e;

			m_dwFront = (m_dwFront + 1) % m_dwSize;

			return true;
		}

	private:
		volatile uint32_t m_dwFront;
		volatile uint32_t m_dwRear;
		SNode *pData;
		volatile uint32_t m_dwSize;
	};

};

#endif
