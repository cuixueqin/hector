/*
 *  ch4_component.cpp
 *  hector
 *
 *  Created by Ben on 05/19/2011.
 *
 */
// changed back to using only concentrations

#include <math.h>
#include "components/ch4_component.hpp"
#include "core/core.hpp"
#include "h_util.hpp"
#include "visitors/avisitor.hpp"

namespace Hector {
  
using namespace std;

//------------------------------------------------------------------------------
/*! \brief Constructor
 */
CH4Component::CH4Component() {
    CH4_emissions.allowInterp( true ); 
    CH4_emissions.name = CH4_COMPONENT_NAME; 
    CH4.allowInterp( true );
    CH4.name = D_ATMOSPHERIC_CH4;
	    
}

//------------------------------------------------------------------------------
/*! \brief Destructor
 */
CH4Component::~CH4Component() {
}

//------------------------------------------------------------------------------
// documentation is inherited
string CH4Component::getComponentName() const {
    const string name = CH4_COMPONENT_NAME;
    
    return name;
}

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::init( Core* coreptr ) {
    logger.open( getComponentName(), false, Logger::DEBUG );
    H_LOG( logger, Logger::DEBUG ) << "hello " << getComponentName() << std::endl;
    core = coreptr;

    // Inform core what data we can provide
    core->registerCapability( D_ATMOSPHERIC_CH4, getComponentName() );
    core->registerCapability( D_PREINDUSTRIAL_CH4, getComponentName() );

    core->registerDependency( D_LIFETIME_OH, getComponentName() ); 
}

//------------------------------------------------------------------------------
// documentation is inherited
unitval CH4Component::sendMessage( const std::string& message,
                                  const std::string& datum,
                                  const message_data info ) throw ( h_exception )
{
    unitval returnval;
    
    if( message==M_GETDATA ) {          //! Caller is requesting data
        return getData( datum, info.date );
        
    } else if( message==M_SETDATA ) {   //! Caller is requesting to set data
        //TODO: call setData below
        //TODO: change core so that parsing is routed through sendMessage
        //TODO: make setData private
        
    } else {                        //! We don't handle any other messages
        H_THROW( "Caller sent unknown message: "+message );
    }
    
    return returnval;
}

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::setData( const string& varName,
                            const message_data& data ) throw ( h_exception )
{
    try {
         if( varName ==  D_PREINDUSTRIAL_CH4  ) {
            H_ASSERT( data.date == Core::undefinedIndex() , "date not allowed" );
            M0 = unitval::parse_unitval( data.value_str, data.units_str, U_PPBV_CH4 );
         } else if( varName == D_EMISSIONS_CH4 ) {
            H_ASSERT( data.date != Core::undefinedIndex(), "date required" );
            CH4_emissions.set( data.date, unitval::parse_unitval( data.value_str, data.units_str, U_TG_CH4 ) );
        } else if( varName == D_LIFETIME_SOIL ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            Tsoil = unitval::parse_unitval( data.value_str, data.units_str, U_YRS );
         } else if( varName == D_LIFETIME_STRAT ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            Tstrat = unitval::parse_unitval( data.value_str, data.units_str, U_YRS );
         } else if( varName == D_CONVERSION_CH4 ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            UC_CH4 = unitval::parse_unitval( data.value_str, data.units_str, U_TG_PPBV );
         } else if( varName == D_NATURAL_CH4 ) {
            H_ASSERT( data.date == Core::undefinedIndex(), "date not allowed" );
            CH4N = unitval::parse_unitval( data.value_str, data.units_str, U_TG_CH4 );
         }
		else {
            H_THROW( "Unknown variable name while parsing " + getComponentName() + ": "
                    + varName );
        }
    } catch( h_exception& parseException ) {
        H_RETHROW( parseException, "Could not parse var: "+varName );
    }
}

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::prepareToRun() throw ( h_exception ) {
    
    H_LOG( logger, Logger::DEBUG ) << "prepareToRun " << std::endl;
	oldDate = core->getStartDate();
    CH4.set(oldDate, M0);  // set the first year's value  
 }

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::run( const double runToDate ) throw ( h_exception ) {
	H_ASSERT( !core->inSpinup() && runToDate-oldDate == 1, "timestep must equal 1" );
  
     
    // modified from Wigley et al, 2002
   const double current_ch4em = CH4_emissions.get( runToDate ).value( U_TG_CH4 ); 
   const double current_toh = core->sendMessage( M_GETDATA, D_LIFETIME_OH, runToDate ).value( U_YRS );
    H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " current_toh = " << current_toh << std::endl;
   
   const double ch4n =  CH4N.value( U_TG_CH4 );
  
    const double emisTocon = current_ch4em + ch4n / UC_CH4.value( U_TG_PPBV ); 
    double previous_ch4 = M0.value( U_PPBV_CH4 ); 
     
    H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " previous CH4 = " << previous_ch4 << std::endl;
    
    if (runToDate!=oldDate)
    {
     previous_ch4 = CH4.get( oldDate );
    }
    
   const double soil_sink = previous_ch4/Tsoil.value( U_YRS );   
   const double strat_sink = previous_ch4/Tstrat.value( U_YRS ); 
   const double oh_sink = previous_ch4/ current_toh;
       
  const double dCH4 = emisTocon - soil_sink - strat_sink - oh_sink; //change in CH4 concentration to be added to previous_ch4
  H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " dCH4 = " << dCH4 << std::endl;

   CH4.set( runToDate, unitval( previous_ch4 + dCH4, U_PPBV_CH4 ) );

   oldDate = runToDate;
   H_LOG( logger, Logger::DEBUG ) << "Year " << runToDate << " CH4 concentraion = " << CH4.get( runToDate ) << std::endl;
}

//------------------------------------------------------------------------------
// documentation is inherited
unitval CH4Component::getData( const std::string& varName,
                              const double date ) throw ( h_exception ) {
    
    unitval returnval;
    
    if( varName == D_ATMOSPHERIC_CH4 ) {
        H_ASSERT( date != Core::undefinedIndex(), "Date required for atmospheric CH4" ); 
        returnval = CH4.get( date );
    } else if( varName == D_PREINDUSTRIAL_CH4 ) {
        H_ASSERT( date == Core::undefinedIndex(), "Date not allowed for preindustrial CH4" );
        returnval = M0;
    } else {
        H_THROW( "Caller is requesting unknown variable: " + varName );
    }
    
    return returnval;
}

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::shutDown() {
	H_LOG( logger, Logger::DEBUG ) << "goodbye " << getComponentName() << std::endl;
    logger.close();
}

//------------------------------------------------------------------------------
// documentation is inherited
void CH4Component::accept( AVisitor* visitor ) {
    visitor->visit( this );
}

}
