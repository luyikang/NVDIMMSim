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

//used to divide the ini parameters so that they don't result in a zero
uint divide_params(uint num, uint denom)
{
    uint temp = (uint)((num / denom) + 0.99f);
    if(temp <= 0)
    {
	return 1;
    }
    
    return temp;
}

uint divide_params(float num, float denom)
{
    uint temp = (uint)((num / denom) + 0.99f);
    if(temp <= 0)
    {
	return 1;
    }
    
    return temp;
}
