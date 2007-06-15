#include <ncbi_pch.hpp>
#include <corelib/ncbitime.hpp>
#include <corelib/ncbi_process.hpp>
#include <corelib/ncbi_system.hpp>
#include <connect/services/netcache_client.hpp>

#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif


USING_NCBI_SCOPE;


static void PrintAllStat();

#ifdef HAVE_SIGNAL_H
/*******************************************************
 * Signals
 *******************************************************/

// not all platforms define sighandler_t

extern "C" {

typedef RETSIGTYPE (*TSigHandler)(int);

static void
ONDATR(TSigHandler handler, int n)
{
    if (handler == SIG_DFL) {
        exit(1);
    } else if (handler != SIG_IGN) {
        handler(n);
    }
}

TSigHandler old_SIGINT;
TSigHandler old_SIGQUIT;
TSigHandler old_SIGALRM;
TSigHandler old_SIGABRT;
TSigHandler old_SIGTERM;
TSigHandler old_SIGBUS;

static void /* RETSIGTYPE */
on_SIGNAL(int OnDatr)
{
    cerr << "PID : " << CProcess::GetCurrentPid() << " : signal caught : "
         << OnDatr << endl;
	PrintAllStat();

	switch(OnDatr) {
		case SIGQUIT : ONDATR(old_SIGQUIT, OnDatr); break;
		case SIGINT  : ONDATR(old_SIGINT, OnDatr); break;
		case SIGALRM : ONDATR(old_SIGALRM, OnDatr); break;
		case SIGABRT : ONDATR(old_SIGABRT, OnDatr); break;
		case SIGTERM : ONDATR(old_SIGTERM, OnDatr); break;
		case SIGBUS  : ONDATR(old_SIGBUS, OnDatr); break;
		default :
			cerr << "J-SIGNAL!" << endl;
			_exit(1);
	}
}

static void
SetSigHandlers()
{
	old_SIGQUIT = signal(SIGQUIT, on_SIGNAL);
	old_SIGINT = signal(SIGINT, on_SIGNAL);
	old_SIGALRM = signal(SIGALRM, on_SIGNAL);
	old_SIGABRT = signal(SIGABRT, on_SIGNAL);
	old_SIGTERM = signal(SIGTERM, on_SIGNAL);
	old_SIGBUS = signal(SIGBUS, on_SIGNAL);
}

}
#endif


/*******************************************************
 * Stressing
 *******************************************************/
class TimePotam {
private :
	static const int EPOCH_V = 10000000;

public :
	TimePotam(size_t MilliSecsWait = 0)
		:	mStartMilliSecs(0)
		,	mLastMilliSecs(0)
		,	mMilliSecsWait(MilliSecsWait)
		{
			mStartMilliSecs = mLastMilliSecs = TimeOfDayMilliSecs();
		};

	size_t FromStart()
		{
			return TimeOfDayMilliSecs() - mStartMilliSecs;
		};

	size_t FromLast()
		{
			size_t cTv = mLastMilliSecs;
			mLastMilliSecs = TimeOfDayMilliSecs();
			return mLastMilliSecs - cTv;
		};

	void WaitTillEnd()
		{
			size_t t2w = TimeOfDayMilliSecs() - mStartMilliSecs;
			if (t2w > mMilliSecsWait) {
				return;
			}

			WaitMilliSecs(mMilliSecsWait - t2w);
		};

	void WaitMilliSecs(size_t MilliSecs)
		{
            SleepMilliSec(MilliSecs);
		};

	size_t TimeOfDayMilliSecs()
		{
            CTime now(CTime::eCurrent, CTime::eGmt);
            size_t Secs = (now.Second() % EPOCH_V) * 1000;
            size_t MilliSecs = now.MilliSecond();
// 
			return Secs + MilliSecs;
		};

private :
	size_t mStartMilliSecs;
	size_t mLastMilliSecs;
	size_t mMilliSecsWait;
};

class DelayPotam {
public :
	DelayPotam(const string &NcKey, Int4 DelayMilliSecs)
		:	mNcKey(NcKey)
		,	mDelayMilliSecs(DelayMilliSecs)
		,	mCreationTime(0)
		{
			mCreationTime = TimePotam().TimeOfDayMilliSecs();
		};

	DelayPotam(const DelayPotam &DPotam)
		:	mNcKey(DPotam.mNcKey)
		,	mDelayMilliSecs(DPotam.mDelayMilliSecs)
		,	mCreationTime(DPotam.mCreationTime)
		{
		};

	DelayPotam &operator = (const DelayPotam &DPotam)
		{
			if (this != &DPotam) {
				mNcKey = DPotam.mNcKey;
				mDelayMilliSecs = DPotam.mDelayMilliSecs;
				mCreationTime = DPotam.mCreationTime;
			}

			return *this;
		};

	bool JustInTime() const
		{
			return TimePotam().TimeOfDayMilliSecs()
							> (mCreationTime + mDelayMilliSecs);
		}
	const string &NcKey() const { return mNcKey; };
	size_t CreationTimeMillisecs() const { return mCreationTime; };
	size_t DelayMilliSecs() const { return mDelayMilliSecs; };

private :
	string mNcKey;
	size_t mDelayMilliSecs;
	size_t mCreationTime;
};

class StressoPotam {
public :
	StressoPotam(
		size_t BufferSize,
		Int4 DelayRatio = 0,
		size_t DelayMilliSecs = 0
	)
		:	mRQty(0)
		,	mREQty(0)
		,	mRTime(0)
		,	mWQty(0)
		,	mWEQty(0)
		,	mWTime(0)
		,	mDQty(0)
		,	mDEQty(0)
		,	mDTime(0)
		,	mBufferSize(BufferSize)
		,	mpBuffer(NULL)
		,	mDelayRatio(DelayRatio)
		,	mDelayMilliSecs(DelayMilliSecs)
		{
			if (mBufferSize == 0) {
				cerr << "Zero Buffer size" << endl;
				abort();
			}
			if ((mpBuffer = new char [mBufferSize]) == NULL) {
				cerr << "Can not allocate " << mBufferSize << " chars" << endl;
				abort();
			}

			*((Int4 *)mpBuffer) = mBufferSize;
		};
	~StressoPotam()
		{
			mRQty = 0;
			mREQty = 0;
			mRTime = 0;
			mWQty = 0;
			mWEQty = 0;
			mWTime = 0;
			mDQty = 0;
			mDEQty = 0;
			mDTime = 0;
			mBufferSize = 0;
			if (mpBuffer != NULL) {
				delete [] mpBuffer;
				mpBuffer = NULL;
			}
			mDelayRatio = 0;
			mDelayMilliSecs = 0;

			mDelayedPotams.clear();
		};

	string Stress(size_t MilliSecsTime, const string &Server, Int4 Port)
		{
			TimePotam TPot(MilliSecsTime);

			string NcKey = "";

			try {
				NcKey = CNetCacheClient(Server, Port, "Ouch!").PutData(mpBuffer, mBufferSize);
				mWTime += TPot.FromLast();
				mWQty ++;

				if (mDelayRatio != 0 && mWQty % mDelayRatio == 0) {
					mDelayedPotams.insert(
									mDelayedPotams.end(),
									DelayPotam(NcKey, mDelayMilliSecs)
								);
				}
			}
			catch( ... ) {
				mWEQty ++;
				NcKey = "";
			}

			if (NcKey != "") {
				try {
					CNetCacheClient NcCli("Ouch!");

					CNetCacheClient::SBlobData Blob;
					CNetCacheClient::EReadResult rres;

					while((rres = NcCli.GetData(NcKey, Blob)) == CNetCacheClient::eReadPart);

					if (rres == CNetCacheClient::eNotFound) {
						mREQty ++;
					}
					else {
						if (*((Int4 *)Blob.blob.get()) == mBufferSize) {
							mRTime += TPot.FromLast();
							mRQty ++;
						}
						else {
							mREQty ++;
						}
					}
				}
				catch( ... ) {
					mREQty ++;
				}
			}

			return NcKey;
		};

	size_t CheckReadDelayed()
		{
			Int4 RetVal = 0;

			while(true) {
				list < DelayPotam >::iterator Pos =
											mDelayedPotams.begin();
				if (Pos == mDelayedPotams.end()) {
					break;
				}

				if (!(*Pos).JustInTime()) {
					break;
				}
				try {
					TimePotam TPot;

					string NcKey = (*Pos).NcKey();

					CNetCacheClient NcCli("Ouch!");

					CNetCacheClient::SBlobData Blob;
					CNetCacheClient::EReadResult rres;

					while((rres = NcCli.GetData(NcKey, Blob)) == CNetCacheClient::eReadPart);

					if (rres == CNetCacheClient::eNotFound) {
						mDEQty ++;
					}
					else {
						if (*((Int4 *)Blob.blob.get()) == mBufferSize) {
							mDTime += TPot.FromStart();
							mDQty ++;
							RetVal ++;
						}
						else {
							mDEQty ++;
						}
					}

				}
				catch( ... ) {
					mDEQty ++;
				}

				mDelayedPotams.erase(Pos);
			}

			return RetVal;
		};

	string Stat()
		{
			string RetVal = NStr::IntToString(mBufferSize) + "(bs)";

			if (mWQty != 0) {
				RetVal += " " + NStr::IntToString(mWQty) + "(wq)";
				size_t AVTime = (size_t)(mWTime / (long long)mWQty);
				RetVal += " " + NStr::IntToString(AVTime) + "(awt)";

			}
			else {
				RetVal += " 0(wq) 0(awt)";
			}
			RetVal += " " + NStr::IntToString(mWEQty) + "(we)";

			if (mRQty != 0) {
				RetVal += " " + NStr::IntToString(mRQty) + "(rq)";
				size_t AVTime = (size_t)(mRTime / (long long)mRQty);
				RetVal += " " + NStr::IntToString(AVTime) + "(art)";

			}
			else {
				RetVal += " 0(rq) 0(art)";
			}
			RetVal += " " + NStr::IntToString(mREQty) + "(re)";

			if (mDQty != 0) {
				RetVal += " " + NStr::IntToString(mDQty) + "(dq)";
				size_t AVTime = (size_t)(mDTime / (long long)mDQty);
				RetVal += " " + NStr::IntToString(AVTime) + "(adt)";

			}
			else {
				RetVal += " 0(dq) 0(adt)";
			}
			RetVal += " " + NStr::IntToString(mDEQty) + "(de)";

			return RetVal;
		};

	size_t RQty() { return mRQty; };
	size_t REQty() { return mREQty; };
	long long RTime() { return mRTime; };
	size_t WQty() { return mWQty; };
	size_t WEQty() { return mWEQty; };
	long long WTime() { return mWTime; };
	size_t DQty() { return mDQty; };
	size_t DEQty() { return mDEQty; };
	long long DTime() { return mDTime; };
	size_t BufferSize() { return mBufferSize; };

private :
	size_t mRQty;
	size_t mREQty;
	long long mRTime;

	size_t mWQty;
	size_t mWEQty;
	long long mWTime;

	size_t mDQty;
	size_t mDEQty;
	long long mDTime;

	size_t mBufferSize;
	char *mpBuffer;

	Int4 mDelayRatio;
	size_t mDelayMilliSecs;
	list < DelayPotam > mDelayedPotams;
};

class StressoPotary {
public :
	StressoPotary()
		:	mServer("")
		,	mPort(0)
		,	mPerSec(100)
		,	mReadDelayMinutes(3)
		,	mDelayRatio(4)
		{
			mStartStressing = TimePotam().TimeOfDayMilliSecs();
			mEndStressing = TimePotam().TimeOfDayMilliSecs();
		};
	~StressoPotary()
		{
			mReadDelayMinutes = 3;
			mDelayRatio = 4;
			mPerSec = 100;
			mServer = "";
			mPort = 0;
			mStartStressing = TimePotam().TimeOfDayMilliSecs();
			mEndStressing = TimePotam().TimeOfDayMilliSecs();

			mPotams.clear();

			while(mPotamMap.size()) {
				StressoPotam *pPotam = (*mPotamMap.begin()).second;
				if (pPotam != NULL) {
					delete pPotam;
				}

				mPotamMap.erase(mPotamMap.begin());
			}
		};

	void Init(
				const string &Server,
				Int4 Port,
				Int4 PerSec = 100,
				Int4 DelayRatio = 0,
				Int4 ReadDelayMinutes = 3
		)
		{
			mServer = Server;
			mPort = Port;
			mPerSec = PerSec;
			mReadDelayMinutes = ReadDelayMinutes;
			mDelayRatio = DelayRatio;

			if (mPerSec < 10 || 400 <= mPerSec) {
				cerr << "Invalid PerSecond value '" << NStr::IntToString(mPerSec) << "' should be less than 400 and greater than 10" << endl;
				abort();
			}
		};

	void AddPotam(size_t BufferSize)
		{
			StressoPotam *pPotam = NULL;

			map < size_t, StressoPotam * >::iterator Pos =
											mPotamMap.find(BufferSize);

			if (Pos == mPotamMap.end()) {
				pPotam = new StressoPotam(
									BufferSize,
									mDelayRatio,
									mReadDelayMinutes * 60 * 1000
								);
				mPotamMap[BufferSize] = pPotam;
			}
			else {
				pPotam = (*Pos).second;
			}

			mPotams.insert(mPotams.end(), pPotam);
		};

	void Stress()
		{
			TimePotam TPot;

			if (mPotams.size() == 0) {
				cerr << "No potams in potary" << endl;
				abort();
			}

			mStartStressing = TPot.TimeOfDayMilliSecs();

			size_t MilliSecsTime = 1000 / mPerSec;

			size_t IterCnt = 0;

			size_t Pupiter = 0;

			for( ; ; ) {
				vector < StressoPotam * >::iterator PB =
														mPotams.begin();
				for( ; PB != mPotams.end(); PB ++) {
					StressoPotam *pPotam = *PB;
					pPotam -> Stress(MilliSecsTime, mServer, mPort);
					mEndStressing = TPot.TimeOfDayMilliSecs();

					IterCnt ++;
				}

				PB = mPotams.begin();
				for( ; PB != mPotams.end(); PB ++) {
					StressoPotam *pPotam = *PB;
					size_t DE = pPotam -> CheckReadDelayed();

						/* OOPS I DID IT lol */
					for(size_t llp = 0; llp < DE; llp++) {
						IterCnt ++;
					}
				}

				if (IterCnt / 10000 != Pupiter) {
					cerr << Stat() << endl;;

					Pupiter = IterCnt / 10000;
				}

				size_t NewT = (IterCnt * 1000) / mPerSec;
				size_t Diffa = TPot.TimeOfDayMilliSecs()
											- mStartStressing;

				if (Diffa < NewT) {
					TPot.WaitMilliSecs(NewT - Diffa);
				}
			}
		};

	string Stat()
		{
			string RetVal = "<==========>\n";

			size_t Qty = 0;
			size_t Err = 0;

			map < size_t, StressoPotam * >::iterator PB = mPotamMap.begin();
			for( ; PB != mPotamMap.end(); PB ++) {
				StressoPotam *pPotam = (*PB).second;
				Qty += pPotam -> RQty();
				Qty += pPotam -> DQty();
				Err += pPotam -> REQty();
				Err += pPotam -> DEQty();
			}

			size_t Diffa = mEndStressing - mStartStressing;

			RetVal += "Blobs read : " + NStr::IntToString(Qty) + "\n";
			RetVal += "All time   : " + NStr::IntToString(Diffa / 1000) + " seconds\n";
			if (Qty != 0) {
				double PSec = 1000 * ((double)Qty / (double)Diffa);
				RetVal += "Per Second : " + NStr::DoubleToString(PSec) + "\n";
			}
			else {
				RetVal += "Per Second : 0\n";
			}
			RetVal += "All errors : " + NStr::IntToString(Err) + "\n";

			PB = mPotamMap.begin();
			for( ; PB != mPotamMap.end(); PB ++) {
				StressoPotam *pPotam = (*PB).second;
				RetVal += "    " + pPotam -> Stat() + "\n";
			}

			return RetVal;
		};

private :
	string mServer;
	Int4 mPort;

	Int4 mPerSec;
	Int4 mReadDelayMinutes;
	Int4 mDelayRatio;

	size_t mStartStressing;
	size_t mEndStressing;

	vector < StressoPotam * > mPotams;
	map < size_t, StressoPotam * > mPotamMap;
};

StressoPotary *ThePotary = NULL;

void
PrintAllStat()
{
	if (ThePotary != NULL) {
		cerr << ThePotary -> Stat() << endl;
	}
}

/****************************************************
 * Main
 ****************************************************/

string theProgName = "stress_2";

// string theConnection = "pdev1:9001";
string theConnection = "";
string theServer = "pdev1";
Int4 thePort = 9001;
Int4 thePerSec = 25;
Int4 theDelayTime = 30;
Int4 theDelayRatio = 4;

string ConnTag   = "-c";
string PerSecTag = "-p";
string DelayTag  = "-d";
string RatioTag  = "-r";

string ConnDsc   = "connection";
string PerSecDsc = "per_second";
string DelayDsc  = "delay_time";
string RatioDsc  = "delay_ratio";

void
usage()
{
	cerr << "Syntax\n";
	cerr << "    " << theProgName;
	cerr << " " << ConnTag << " " << ConnDsc;
//	cerr << " [" << ConnTag << " " << ConnDsc << "]";
	cerr << " [" << PerSecTag << " " << PerSecDsc << "]";
	cerr << " [" << DelayTag << " " << DelayDsc << "]";
	cerr << " [" << RatioTag << " " << RatioDsc << "]";
	cerr << "\n";
	cerr << "Where\n";
	cerr << "    " << ConnDsc << "  - connection string (required)\n";
	cerr << "    " << PerSecDsc << "  - blobs per second (25)\n";
	cerr << "    " << DelayDsc << "  - delay minutes for secondary reading (30 minutes)\n";
	cerr << "    " << RatioDsc << " - delay ratio for secondary reading (4)\n";
	cerr << "\n";
}

bool
parseArgs(int argc, char **argv)
{
	theProgName = *argv;
	for(int llp = 1; llp < argc; llp ++) {
		string Arg = argv[llp];

		if (Arg == ConnTag) {
			llp ++;
			if (llp >= argc) {
				cerr << ConnDsc << " parameter missed" << endl;
				return false;
			}
			theConnection = argv[llp];
			continue;
		}

		if (Arg == PerSecTag) {
			llp ++;
			if (llp >= argc) {
				cerr << PerSecDsc << " parameter missed" << endl;
				return false;
			}
			thePerSec = NStr::StringToInt(argv[llp]);
			continue;
		}

		if (Arg == DelayTag) {
			llp ++;
			if (llp >= argc) {
				cerr << DelayDsc << " parameter missed" << endl;
				return false;
			}
			theDelayTime = NStr::StringToInt(argv[llp]);
			continue;
		}

		if (Arg == RatioTag) {
			llp ++;
			if (llp >= argc) {
				cerr << RatioDsc << " parameter missed" << endl;
				return false;
			}
			theDelayRatio = NStr::StringToInt(argv[llp]);
			continue;
		}

		cerr << "Unknown parameter '" << Arg << "'" << endl;
		return false;
	}

	if (theConnection == "") {
		cerr << "Connection string parameter undefined" << endl;
		return false;
	}

	size_t pos = theConnection.find(":");
	if (pos == string::npos) {
		cerr << "Invalid connection string format : '" << theConnection << "' should be : host:port" << endl;
		return false;
	}

	theServer = theConnection.substr(0, pos);
	string PortStr = theConnection.substr(pos + 1);

	thePort = 0;
	try {
		thePort = NStr::StringToInt(PortStr);
	}
	catch( ... ) {
		thePort = 0;
	}

	if (thePort <= 0) {
		cerr << "Invalid port value : '" << PortStr << "' should be integer greater than 0 and less than 7" << endl;
		return false;
	}

	// cerr << "NetCache : " << theServer << ":" << thePort << endl;
	// cerr << "PerSec   : " << thePerSec << endl;
	// cerr << "DelayTm  : " << theDelayTime << endl;
	// cerr << "DelayRt  : " << theDelayRatio << endl;

	return true;
}

int
main(int argc, char **argv)
{
	if (!parseArgs(argc, argv)) {
		usage();
		return 1;
	}

#ifdef HAVE_SIGNAL_H
	SetSigHandlers();
#endif

	cerr << "Press <Cntrl-C> to inerrupt" << endl;

	ThePotary = new StressoPotary;

	ThePotary -> Init(
					theServer,
					thePort,
					thePerSec,
					theDelayRatio,
					theDelayTime
					);

	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(4000);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(8000);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(16000);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(32000);
	ThePotary -> AddPotam(490);
	ThePotary -> AddPotam(64000);
	ThePotary -> AddPotam(490);

	ThePotary -> Stress();

	cerr << ThePotary -> Stat() << endl;

	delete ThePotary;
	ThePotary = NULL;

	return 0;
}
