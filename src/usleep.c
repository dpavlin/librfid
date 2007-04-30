#include <windows.h>
#include <errno.h>

void usleep(unsigned long usec)
{
    int msec;

    msec=usec/1000;
    usec=usec%1000;    
    
    if(msec)
    {
	if(usec)
	    msec++;
    }
    else
    {
	LARGE_INTEGER current,freq,end;
	
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&end);
	
	end.QuadPart+=(freq.QuadPart*usec)/1000000;
	while(QueryPerformanceCounter(&current) && (current.QuadPart<=end.QuadPart))
	{
	}	
    }
	
    Sleep(msec);
}
