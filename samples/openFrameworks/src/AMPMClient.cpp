#include "ofMain.h"
#include "AMPMClient.h"

#include <unordered_map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <cstring>

#ifdef _WIN32
	#include <windows.h>
#endif

using namespace std;

namespace ampm {

	const unordered_map<ampm::LogEventLevel, const char*> LogEventLevelToString = {
		{ ampm::LogEventLevel::AMPM_INFO, "info" },
		{ ampm::LogEventLevel::AMPM_WARNING, "warn" },
		{ ampm::LogEventLevel::AMPM_ERROR, "error" } };

	// Cross-platform debug output helper: uses OutputDebugStringA on Windows,
	// and std::cerr on macOS / Linux to avoid calling ofLog() from inside our logger.
	inline void debugOutput(const std::string& msg) {
	#ifdef _WIN32
		OutputDebugStringA(msg.c_str());
	#else
		std::cerr << msg;
	#endif
	}

	// ----------------------
	// logging
	// ----------------------
	LogEventLevel getLogLevel(ofLogLevel level)
	{
		switch (level) {
		case OF_LOG_VERBOSE:
			return LogEventLevel::AMPM_INFO;
		case OF_LOG_NOTICE:
			return LogEventLevel::AMPM_INFO;
		case OF_LOG_WARNING:
			return LogEventLevel::AMPM_WARNING;
		case OF_LOG_ERROR:
			return LogEventLevel::AMPM_ERROR;
		case OF_LOG_FATAL_ERROR:
			return LogEventLevel::AMPM_ERROR;
		case OF_LOG_SILENT:
			return LogEventLevel::AMPM_INFO;
		default:
			break;
		}
		return LogEventLevel::AMPM_ERROR;
	}

	AMPMLoggerChannel::AMPMLoggerChannel()
	{
	}

	AMPMLoggerChannel::~AMPMLoggerChannel()
	{
	}

	void AMPMLoggerChannel::log(ofLogLevel level, const std::string& module, const std::string& message)
	{
		if (isAMPMLoggingLevel(level)) {
			// send to AMPM server (this should not re-enter ofLog)
			ampm::ampm()->log(getLogLevel(level), "[" + ofGetLogLevelName(level, false) + "] " + module + ": " + message);
		}
		else {
			// print to stderr/stdout directly *without* calling ofLog() to avoid recursion
			std::stringstream out;
			out << "[" << ofGetLogLevelName(level, false) << "] ";
			if (!module.empty()) {
				out << module << ": ";
			}
			out << message << std::endl;

			// On Windows use OutputDebugStringA, else use cerr
			debugOutput(out.str());
		}
	}

	void AMPMLoggerChannel::log(ofLogLevel level, const std::string& module, const char* format, ...)
	{
		va_list args;
		va_start(args, format);
		log(level, module, format, args);
		va_end(args);
	}

	void AMPMLoggerChannel::log(ofLogLevel level, const std::string& module, const char* format, va_list args)
	{
		std::string buffer;
		buffer = "[" + ofGetLogLevelName(level, false) + "] ";
		if (!module.empty()) {
			buffer += module + ": ";
		}
		buffer += ofVAArgsToString(format, args);
		if (buffer.empty() || buffer.back() != '\n') {
			buffer += "\n";
		}

		if (isAMPMLoggingLevel(level)) {
			// send to AMPM server
			ampm::ampm()->log(getLogLevel(level), buffer);
		}
		else {
			// direct debug output to avoid recursion into ofLog
			debugOutput(buffer);
		}
	}

	bool AMPMLoggerChannel::isAMPMLoggingLevel(ofLogLevel level)
	{
		return std::find(m_levelsToLog.begin(), m_levelsToLog.end(), level) != m_levelsToLog.end();
	}

	// ----------------------
	// client
	// ----------------------

	AMPMClient* AMPMClient::sInstance = nullptr;

	void AMPMClient::init(int sendPort, int recvPort, int serverPORT)
	{
		sInstance = new AMPMClient(sendPort, recvPort, serverPORT);
	}

	void AMPMClient::set(AMPMClient* instance)
	{
		sInstance = instance;
	}

	AMPMClient* AMPMClient::get()
	{
		return sInstance;
	}

	AMPMClient::~AMPMClient()
	{
		sInstance = nullptr;
	}

	AMPMClient::AMPMClient(int sendPort, int recvPort, int serverPORT)
	{
		// setup osc
		mSender.setup("localhost", sendPort);

		mListener.setup(recvPort);

		m_serverPort = serverPORT;

		std::shared_ptr<AMPMLoggerChannel> ampmLogChannel_p = std::make_shared<AMPMLoggerChannel>();
		ofSetLoggerChannel(ampmLogChannel_p);
	}

	ofJson AMPMClient::getConfig()
	{
		ofJson config;

		try {
			// Use ofLoadURL to fetch from HTTP — cross platform. ofLoadJson for local files only.
			std::string url = "http://localhost:" + ofToString(m_serverPort) + "/config";
			ofHttpResponse resp = ofLoadURL(url);
			if (resp.status == 200) {
				// Parse JSON from response.data
				try {
					config = ofJson::parse(resp.data);
				}
				catch (const std::exception& ex) {
					ofLogError() << "Failed to parse JSON from " << url << ": " << ex.what();
				}
			}
			else {
				ofLogWarning() << "Failed to fetch config from " << url << " (status " << resp.status << ")";
			}
		}
		catch (const std::exception& ex) {
			ofLogFatalError() << ex.what();
		}

		return config;
	}

	void AMPMClient::update()
	{
		// send heartbeat
		sendHeartbeat();
	}

	// send heartbeat to server
	void AMPMClient::sendHeartbeat()
	{
		ofxOscMessage message;
		message.setAddress("/heart");
		mSender.sendMessage(message, false);
	}

	// send analytics event to server
	void AMPMClient::sendEvent(std::string category, std::string action, std::string label, int value)
	{
		ofxOscMessage message;
		message.setAddress("/event");

		ofJson arguments = ofJson{ { "Category", category }, { "Action", action }, { "Label", label }, { "Value", value } };
		message.addStringArg(arguments.dump());

		mSender.sendMessage(message);
	}

	// send log event to server
	void AMPMClient::log(LogEventLevel level, std::string msg)
	{
		ofxOscMessage message;
		message.setAddress("/log");

		ofJson arguments = ofJson{ { "level", LogEventLevelToString.at(level) }, { "message", msg } };
		message.addStringArg(arguments.dump());
		mSender.sendMessage(message);
	}

	// send custom osc message
	void AMPMClient::sendCustomMessage(std::string address, ofJson msg)
	{
		ofxOscMessage message;
		message.setAddress(address);
		message.addStringArg(msg.dump());
		mSender.sendMessage(message);
	}

	// strip out file for sending as part of log info (works with both '/' and '\')
	char const* AMPMClient::getFileForLog(char const* file)
	{
		if (!file) return file;
		const char* slash = strrchr(file, '/');
		const char* backslash = strrchr(file, '\\');

		const char* sep = nullptr;
		if (slash && backslash) {
			sep = (slash > backslash) ? slash : backslash;
		}
		else if (slash) {
			sep = slash;
		}
		else if (backslash) {
			sep = backslash;
		}

		return sep ? sep + 1 : file;
	}

}  // namespace ampm

