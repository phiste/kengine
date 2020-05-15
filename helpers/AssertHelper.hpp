#pragma once

#ifdef WIN32
# define kengine_assume(x) __assume(x)
#else
# define kengine_assume(x) (void)0
#endif

#ifdef kengine_ndebug
# define kengine_assert(em, x) (void)0
# define kengine_assert_failed(em, x) (void)0
# define kengine_debug_break (void)0
#else
# ifdef WIN32
#  define kengine_debug_break DebugBreak()
# else
#  define kengine_debub_break (void)0
# endif
# define kengine_assert_failed(em, x) \
	do {\
		kengine::AssertHelper::assertFailed(em, __FILE__, __LINE__, x); \
		if (kengine::AssertHelper::isDebuggerPresent()) \
			kengine_debug_break; \
	} while(false)
# define kengine_assert(em, x) \
	do {\
		if (!!(x)) \
			(void)0; \
		else \
			kengine_assert_failed(em, #x); \
		kengine_assume(!!(x)); \
	} while (false)
#endif

namespace kengine {
	class EntityManager;

	namespace AssertHelper {
		void assertFailed(EntityManager & em, const char * file, int line, const char * expr);
		bool isDebuggerPresent();
	}
}