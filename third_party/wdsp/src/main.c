/*  main.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/

// NereusSDR modifications (2026-09-23, J.J. Boyd KG4VCF, with Anthropic
// Claude Code): the worker takes and releases csDSP for each block through
// dsplock.c (WdspWorkerEnter/WdspWorkerLeave) so waiting control calls get a
// bounded turn. After its loop ends the worker signals channel teardown
// through dsplock.c (WdspWorkerExited), so pre_main_destroy frees nothing
// while a block is still running. A processed block starts with dsplock.c's
// test-only process delay (WdspWorkerTestProcessDelay; off by default).
// Source DSP flow and all upstream attribution are retained.

#include "comm.h"

void wdspmain (void *pargs)
{
	DWORD taskIndex = 0;
	HANDLE hTask = AvSetMmThreadCharacteristics(TEXT("Pro Audio"), &taskIndex);
	if (hTask != 0) AvSetMmThreadPriority(hTask, 2);
	else SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	int channel = (int)(uintptr_t)pargs;
	while (_InterlockedAnd (&ch[channel].run, 1))
	{
		WaitForSingleObject(ch[channel].iob.pd->Sem_BuffReady,INFINITE);
		WdspWorkerEnter (channel);
		if (!_InterlockedAnd (&ch[channel].iob.pd->exec_bypass, 1))
		{
			WdspWorkerTestProcessDelay (channel);
			switch (ch[channel].type)
			{
			case 0:		// rxa
				dexchange (channel, rxa[channel].outbuff, rxa[channel].inbuff);
				xrxa (channel);
				break;
			case 1:		// txa
				dexchange (channel, txa[channel].outbuff, txa[channel].inbuff);
				xtxa (channel);
				break;
			case 31:	//

				break;
			}
		}
		WdspWorkerLeave (channel);
	}
	if (hTask != 0) AvRevertMmThreadCharacteristics (hTask);
	WdspWorkerExited (channel);
}

void create_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		create_rxa (channel);
		break;
	case 1:
		create_txa (channel);
		break;
	case 31:  //
		
		break;
	}
}

void destroy_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		destroy_rxa (channel);
		break;
	case 1:
		destroy_txa (channel);
		break;
	case 31:  //
		
		break;
	}
}

void flush_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		flush_rxa (channel);
		break;
	case 1:
		flush_txa (channel);
		break;
	case 31:
		
		break;
	}
}

void setInputSamplerate_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		setInputSamplerate_rxa (channel);
		break;
	case 1:
		setInputSamplerate_txa (channel);
		break;
	case 31:  //

		break;
	}
}

void setOutputSamplerate_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		setOutputSamplerate_rxa (channel);
		break;
	case 1:
		setOutputSamplerate_txa (channel);
		break;
	case 31:  //

		break;
	}
}

void setDSPSamplerate_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		setDSPSamplerate_rxa (channel);
		break;
	case 1:
		setDSPSamplerate_txa (channel);
		break;
	case 31:  //

		break;
	}
}

void setDSPBuffsize_main (int channel)
{
	switch (ch[channel].type)
	{
	case 0:
		setDSPBuffsize_rxa (channel);
		break;
	case 1:
		setDSPBuffsize_txa (channel);
		break;
	case 31:  //

		break;
	}
}
