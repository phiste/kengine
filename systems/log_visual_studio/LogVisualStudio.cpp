#include "LogVisualStudio.hpp"

#include <mutex>

#include "kengine.hpp"
#include "data/AdjustableComponent.hpp"
#include "functions/Log.hpp"
#include "magic_enum.hpp"
#include "thread_name.hpp"

namespace kengine {
	EntityCreator * LogVisualStudioSystem() noexcept {
		return [](Entity & e) noexcept {
			auto & severityControl = e.attach<LogSeverityControl>();

			e += AdjustableComponent{
				"Log", {
					{ "Visual Studio Console", &severityControl.severity }
				}
			};

			e += functions::Log{
				[&](LogSeverity severity, const char * category, const char * message) noexcept {
					if (severity < severityControl.severity)
						return;

					putils::string<4096> s;

					const auto & threadName = putils::get_thread_name();
					if (!threadName.empty())
						s += '{' + threadName + "}\t";
					s += putils::magic_enum::enum_name<LogSeverity>(severity);
					s += "\t[";
					s += category;
					s += "]\t";
					s += message;
					s += '\n';

					static std::mutex mutex;
					const std::lock_guard lock(mutex);
					OutputDebugStringA(s.str().c_str());
				}
			};
		};
	}
}