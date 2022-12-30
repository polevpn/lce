#ifndef __LCE_CH2SHORTT3_H
#define __LCE_CH2SHORTT3_H

namespace lce
{
	class CH2ShortT3PackageFilter : public CPackageFilter
	{

		enum
		{
			PKG_HEAD_MARK = 0x02,
			PKG_TAIL_MARK = 0x03,
		};
		enum
		{
			MAX_PKG_SIZE = (unsigned short)0xffff,
		};

	public:
		// return value: -2 invalid pkt, -1 pkt haven't received whole pkt, 0 normal
		inline int isWholePkg(const char *pszData, const int iDataSize, int &iRealPkgLen, int &iPkgLen)
		{
			int iRe = -2;
			if (iDataSize <= 0)
			{
				return -1;
			}

			if (iDataSize <= 3)
			{
				iRe = -1;
				if (*pszData != PKG_HEAD_MARK)
				{
					iRe = -2;
				}
			}
			else
			{
				if (*pszData != PKG_HEAD_MARK)
				{
					return -2;
				}

				iPkgLen = ntohs(*((unsigned short *)(pszData + 1)));
				if (iDataSize >= iPkgLen && iPkgLen >= 4)
				{

					if (*(pszData + iPkgLen - 1) == PKG_TAIL_MARK)
					{
						iRealPkgLen = iPkgLen - 4;
						iRe = 0;
					}
					else
					{
						iRe = -2;
					}
				}
				else
				{
					iRe = -1;
					if (*pszData != PKG_HEAD_MARK || iPkgLen < 4)
					{
						iRe = -2;
					}
				}
			}
			return iRe;
		}

		const char *getRealPkgData(const char *pszData, const int iDataSize)
		{
			return pszData + 3;
		}
	};

};

#endif
