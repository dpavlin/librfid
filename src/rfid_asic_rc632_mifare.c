
int
rc632_mifare_read16()
{
	unsigned char sndbuf[2];
	unsigned char recvbuf[0x40];
	unsigned char recvlen = sizeof(recvbuf);
	
	int ret;

	sndbuf[0] = 0x30;
	sndbuf[1] = arg_4;

	memset(recvbuf, 0, sizeof(recvbuf));

	ret = rc632_transcieve(handle, sndbuf, sizeof(sndbuf), 
				recvbuf, &recvlen, 0x32, 0);
	if (ret < 0)
		return ret;

	if (recvlen != 0x10)
		return -1;

	return 0;
}
