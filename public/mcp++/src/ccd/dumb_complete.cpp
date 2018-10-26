extern "C"
{
	int net_complete_func(const void* data, unsigned data_len)
	{
		if(data_len < (1<<22))
			return 0;
		else
			return -1;
	}
}
