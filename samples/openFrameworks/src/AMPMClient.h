#pragma once

#include "ofJson.h"
#include "ofxOsc.h"
#include <queue>
#include <memory>
#include <vector>
#include <string>

namespace ampm {

	enum LogEventLevel {
		AMPM_INFO = 1,
		AMPM_ERROR,
		AMPM_WARNING,
		AMPM_SILENT = 0
	};

	class AMPMLoggerChannel : public ofBaseLoggerChannel
	{
	public:
		AMPMLoggerChannel();
		~AMPMLoggerChannel();

	protected:
		// Matches exactly the virtual methods in ofBaseLoggerChannel (oF 0.12.1)
		void log(ofLogLevel level, const std::string& module, const std::string& message) override;
		void log(ofLogLevel level, const std::string& module, const char* format, ...) OF_PRINTF_ATTR(4, 5) ;
		void log(ofLogLevel level, const std::string& module, const char* format, va_list args) ;

	private:
		bool isAMPMLoggingLevel(ofLogLevel level);
		std::vector<ofLogLevel> m_levelsToLog = { OF_LOG_ERROR, OF_LOG_FATAL_ERROR };
	};

	typedef std::shared_ptr<class AMPMClient> AMPMClientRef;

	class AMPMClient
	{
		static AMPMClient* sInstance;

		ofxOscSender mSender;
		ofxOscReceiver mListener;

	public:
		static AMPMClient* get();
		static void init(int sendPort, int recvPort, int serverPORT);
		static void set(AMPMClient* instance);

		~AMPMClient();

		ofJson getConfig();
		void update();
		void sendEvent(std::string category = "", std::string action = "", std::string label = "", int value = 0);
		void sendCustomMessage(std::string address, ofJson msg);
		void log(LogEventLevel level, std::string msg);
		static char const* getFileForLog(char const* file);

	protected:
		AMPMClient(int sendPort, int recvPort, int serverPORT);
		void sendHeartbeat();

	private:
		int m_serverPort = 8888;
	};

	inline AMPMClient* ampm()
	{
		return AMPMClient::get();
	}

	// log macros
	#define AMPM_LOG(M)       AMPMClient::get()->log(ampm::LogEventLevel::AMPM_INFO, M)
	#define AMPM_LOG_ERR(M)   AMPMClient::get()->log(ampm::LogEventLevel::AMPM_ERROR, M)
	#define AMPM_LOG_WARN(M)  AMPMClient::get()->log(ampm::LogEventLevel::AMPM_WARNING, M)

}  // namespace ampm

