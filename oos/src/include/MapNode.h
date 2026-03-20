#ifndef MAP_NODE_H
#define MAP_NODE_H

/*
 *@comment ����ṹ��ӦUnixv6�е�map�ṹ
 *�������map�ṹ�ο�
 * struct map	@line 2515
 * {
	char *m_size;
	char *m_addr;
 * }
 */
struct MapNode
{
	unsigned long m_Size;
	/*
	 * ע�Ϳ����ǲ��Եġ�
	 * m_addr ��ʾ���ݿ��������ռ��е�����λ�ã�
	 * ����physical�ڴ���4kһ�飬��m_AddressIdxΪ2��
	 * ���ʾ0x2000(8k)��λ�ã�ͬ��swap���У�
	 * ���ݿ��СΪ512byte
	 */
	unsigned long m_AddressIdx;	     //����ռ����ʼ��ַ
};

// NOTE:1
#define M_PAGE_SIZE 4096

class BitMap
{
public:
	unsigned long m_AddressIdx; // ����ռ����ʼ��ַ, ����
	int rows;
	unsigned long long int map[256]; // TODO��ʵ�ִ�С������Ҫ����
	void set(unsigned long startAddr, unsigned long page_num)
	{
		m_AddressIdx = startAddr;
		rows = page_num / 64;
		for (int i = 0; i < rows; i++)
		{
			map[i] = 0;
		}
	};
};

#endif

