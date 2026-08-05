#ifndef CGE_EVENT_H
#define CGE_EVENT_H

#include <atomic>
#include <unordered_map>
#include <string>
#include <typeinfo>
#include <stdexcept>

namespace cge::event
{
	using ChannelTag = std::string;
	using ChannelId = size_t;
	
	class EventBase
	{
	public:
		EventBase() = default;
		virtual ~EventBase() = default;
	};

	template<typename PayloadType>
	class Event : public EventBase
	{
	public:
		PayloadType payload;
		Event(const PayloadType &payload) : payload(payload) {}
	};

	class EventChannelBase
	{
		ChannelId m_id;
		static ChannelId nextId()
		{
			static std::atomic<ChannelId> id(0); return id.fetch_add(1, std::memory_order_relaxed);
		}

	protected:
		EventChannelBase() : m_id(nextId()) {}
	public:

		virtual ~EventChannelBase() = default;
		ChannelId id() const { return m_id; }
	};

	template<typename PayloadType>
	class EventChannel : public EventChannelBase
	{
	public:
		virtual ~EventChannel() = default;
	protected:
		EventChannel() = default;
		friend class EventChannelRegistry;
	};

	class EventChannelRegistry
	{
	public:
		using ChannelMapIter = std::unordered_map<ChannelTag, EventChannelBase *>::iterator;

		EventChannelRegistry() = default;
		~EventChannelRegistry();

		// disable copy and assignment
		EventChannelRegistry(const EventChannelRegistry &) = delete;
		EventChannelRegistry &operator=(const EventChannelRegistry &) = delete;
		//custome move constructor and assignment operator
		EventChannelRegistry(EventChannelRegistry &&other) noexcept : m_channels(std::move(other.m_channels)) {}
		EventChannelRegistry &operator=(EventChannelRegistry &&other) noexcept;

		// Raise if the channel already exists with unmatched payload type
		template<typename PayloadType>
		EventChannel<PayloadType>& getChannel(const ChannelTag &name)
		{
			ChannelMapIter it = m_channels.find(name);
			if(it != m_channels.end())
			{
				// Check if the existing channel has the same payload type with typeid
				EventChannelBase *existingChannel = it->second;
				if(typeid(*existingChannel) != typeid(EventChannel<PayloadType>))
				{
					throw std::runtime_error("Type error: attempted to re-register channel with a different payload type");
				}
				return *static_cast<EventChannel<PayloadType> *>(existingChannel);
			}
			EventChannel<PayloadType> *newChannel = new EventChannel<PayloadType>();
			m_channels[name] = newChannel;
			return *newChannel;
		}

	private:
		std::unordered_map<ChannelTag, EventChannelBase*> m_channels;

		void destroyMap() noexcept;
	};
}

#endif // CGE_EVENT_H