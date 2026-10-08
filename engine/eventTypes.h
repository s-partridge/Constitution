#ifndef CGE_EVENT_TYPES_H
#define CGE_EVENT_TYPES_H

#include <ostream>

namespace cge::event
{
	enum class DispatchStatus
	{
		Success,	// Event/Command executed successfully
		Failure,	// Event/Command failed due to an unknown error
		Duplicate,	// Illegal submission of a duplicate command, not used by events
		Pending,	// Event/Command was successfully queued and will be completed later
		NotReady,	// Event/Command was rejected because the dispatcher was not ready to receive it
		Invalid,	// Command was rejected because it was not a valid command, not used by events
		BadInput,	// Command was rejected because the input was not valid, not used by events
		WrongThread // Event/Command was rejected because it was sent from an invalid thread
	};

	inline std::ostream &operator<<(std::ostream &os, const DispatchStatus &rhs)
	{
		switch(rhs)
		{
		case DispatchStatus::Success:
			os << "Success";
			break;
		case DispatchStatus::Failure:
			os << "Failure";
			break;
		case DispatchStatus::Duplicate:
			os << "Duplicate";
			break;
		case DispatchStatus::Pending:
			os << "Pending";
			break;
		case DispatchStatus::NotReady:
			os << "NotReady";
			break;
		case DispatchStatus::Invalid:
			os << "Invalid";
			break;
		case DispatchStatus::BadInput:
			os << "BadInput";
			break;
		case DispatchStatus::WrongThread:
			os << "WrongThread";
			break;
		default:
			os << "BadValue";
		}
		return os;
	}
}

#endif