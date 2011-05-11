#include "Util.h"

//mostly shamelessly stolen from HybridSim
uint64_t convert_uint64_t(string value)
{
	// Check that each character in value is a digit.
	for(size_t i = 0; i < value.size(); i++)
	{
		if(!isdigit(value[i]))
		{
			cout << "ERROR: Non-digit character found: " << value << "\n";
			abort();
		}
	}

	// Convert it
	stringstream ss;
	uint64_t var;
	ss << value;
	ss >> var;

	return var;
}
