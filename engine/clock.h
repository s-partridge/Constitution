#ifndef CGE_CLOCK_H
#define CGE_CLOCK_H

class Clock
{
	// Base update rate in updates per second. This is the rate at which the engine will attempt to update.
	unsigned int m_baseTickRate;
	// Total number of ticks since the clock started.
	unsigned int m_tickCount = 0;
	// Time scale factor for slow motion or fast forward effects.
	double m_timeScale = 1.0;

public:
	Clock(unsigned int baseTickRate = 60, double timeScale = 1.0) : m_baseTickRate(baseTickRate), m_timeScale(timeScale) {}

	unsigned int getBaseTickRate() const { return m_baseTickRate; }
	void setBaseTickRate(unsigned int baseTickRate) { m_baseTickRate = baseTickRate; }

	double getTimeScale() const { return m_timeScale; }
	void setTimeScale(double timeScale) { m_timeScale = timeScale; }
};

#endif