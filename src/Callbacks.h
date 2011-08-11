
#ifndef NVDSIMCALLBACK_H
#define NVDSIMCALLBACK_H

namespace NVDSim {

template <typename ReturnT, typename Param1T, typename Param2T, 
    typename Param3T, typename Param4T>
class CallbackBase
{
    public:
        virtual ~CallbackBase() = 0;
        virtual ReturnT operator()(Param1T, Param2T, Param3T, Param4T) = 0;
};

template <typename Return, typename Param1T, typename Param2T, typename Param3T, typename Param4T>
    NVDSim::CallbackBase<Return,Param1T,Param2T,Param3T,Param4T>::~CallbackBase(){}

template <typename ConsumerT, typename ReturnT, 
    typename Param1T, typename Param2T, typename Param3T, typename Param4T >
class Callback: public CallbackBase<ReturnT,Param1T,Param2T,Param3T,Param4T>
{
    private:
    typedef ReturnT (ConsumerT::*PtrMember)(Param1T,Param2T,Param3T,Param4T); 

    public:
        Callback( ConsumerT* const object, PtrMember member) :
                object(object), member(member) {
        }

Callback( const Callback<ConsumerT,ReturnT,Param1T,Param2T,Param3T,Param4T>& e ) :
                object(e.object), member(e.member) {
        }

		ReturnT operator()(Param1T param1, Param2T param2, Param3T param3, Param4T param4) {
		    return (const_cast<ConsumerT*>(object)->*member)(param1,param2,param3,param4);
        }

    private:

        ConsumerT* const object;
        const PtrMember  member;
};

} // namespace FDSim 

#endif
