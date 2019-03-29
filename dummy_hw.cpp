#include "hardware.h"
#include "hwconfig.h"
#include "logging.h"

class DummyRelay : public Relay
{
public:
	DummyRelay() : Relay(false)
	{}

protected:
	virtual void ApplyState(bool on) override
	{}
};

class DummySwitch : public Switch
{
public:
	DummySwitch(bool inverted) : Switch(inverted)
	{}

public:
	virtual int GetState() override
	{
		return m_activeLow ? On : Off;
	}
};

//*** XML deserializers begin here ***

REGISTER_DEVICE_TYPE(DummyRelay)(xmlNode *node, HWConfig *cfg)
{
	return new DummyRelay();
}

REGISTER_DEVICE_TYPE(DummySwitch)(xmlNode *node, HWConfig *cfg)
{
	int inverted = GetIntProp(node, "inverted", 0);

	if (inverted == -1) {
		return nullptr;
	} else {
		return new DummySwitch(inverted);
	}
}
