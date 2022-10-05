namespace lg2
{
	static constexpr unsigned RoundUp(unsigned v)
	{
		unsigned cur = v;
		unsigned lg2 = 0;

		while (cur >>= 1)
		{
			++lg2;
		}

		if ((v & (v - 1)) != 0)
		{
			++lg2;
		}

		return lg2;
	}
}