/*! 
* \file resource.cpp
* \ingroup Objects
* \brief Resource class source file.
* \author Sonny Kim
* \date $Date$
* \version $Revision$
*/

#include "util/base/include/definitions.h"

#include <string>
#include <iostream>
#include <fstream>
#include <vector>
#include <cassert>
#include <xercesc/dom/DOMNode.hpp>
#include <xercesc/dom/DOMNodeList.hpp>

// class headers
#include "util/base/include/xml_helper.h"
#include "resources/include/resource.h"
#include "resources/include/subresource.h"
#include "containers/include/scenario.h"
#include "util/base/include/model_time.h"
#include "marketplace/include/marketplace.h"
#include "marketplace/include/imarket_type.h"
#include "resources/include/resource.h"
#include "resources/include/renewable_subresource.h"
#include "containers/include/iinfo.h"

using namespace std;
using namespace xercesc;

extern Scenario* scenario;
// static initialize.
const string DepletableResource::XML_NAME = "depresource";
const string FixedResource::XML_NAME = "fixedresource";
const string RenewableResource::XML_NAME = "renewresource";

//! Default constructor.
Resource::Resource(){
	nosubrsrc = 0;
	// resize vectors not read in
	const Modeltime* modeltime = scenario->getModeltime();
	const int maxper = modeltime->getmaxper();
	available.resize(maxper); // total resource availabl
	annualprod.resize(maxper); // annual production rate of resource
	cummprod.resize(maxper); // cummulative production of resource
	rscprc.resize( maxper ); 
}

//! Destructor.
Resource::~Resource() {
	for ( vector<SubResource*>::iterator iter = subResource.begin(); iter != subResource.end(); iter++ ) {
		delete *iter;
	}
}

//! Set data members from XML input.
void Resource::XMLParse( const DOMNode* node ){

	const Modeltime* modeltime = scenario->getModeltime();
	string nodeName;
	DOMNodeList* nodeList = 0;
	DOMNode* curr = 0;

	// make sure we were passed a valid node.
	assert( node );

	// get the name attribute.
	name = XMLHelper<string>::getAttrString( node, "name" );

	// get all child nodes.
	nodeList = node->getChildNodes();

	// loop through the child nodes.
	for( int i = 0; i < static_cast<int>( nodeList->getLength() ); i++ ){
		curr = nodeList->item( i );
		nodeName = XMLHelper<string>::safeTranscode( curr->getNodeName() );

		if( nodeName == "#text" ) {
			continue;
		}

		else if( nodeName == "market" ){
			market = XMLHelper<string>::getValueString( curr ); // only one market element.
		}
		else if( nodeName == "price" ){
			XMLHelper<double>::insertValueIntoVector( curr, rscprc, modeltime );
		}
		else if( XMLDerivedClassParse( nodeName, curr ) ){
		}
		else {
			cout << "Unrecognized text string: " << nodeName << " found while parsing Resource." << endl;
		}
	}
}

/*! \brief Complete the initialization
*
* This routine is only called once per model run
*
* \author Josh Lurz
* \warning markets are not necesarilly set when completeInit is called
*/

void Resource::completeInit( const string& aRegionName ) {
    nosubrsrc = static_cast<int>( subResource.size() );

	for( vector<SubResource*>::iterator subResIter = subResource.begin(); subResIter != subResource.end(); subResIter++ ) {
		( *subResIter )->completeInit();
	}

    // Set markets for this sector
    setMarket( aRegionName );
}

//! Write datamembers to datastream in XML format for replicating input file.
void Resource::toInputXML( ostream& out, Tabs* tabs ) const {
	XMLWriteOpeningTag( getXMLName(), out, tabs, name );

	// write the xml for the class members.
	XMLWriteElement( market, "market", out, tabs );

	// write out resource prices for base period only
	const Modeltime* modeltime = scenario->getModeltime();
	XMLWriteElement( rscprc[ 0 ], "price", out, tabs, modeltime->getper_to_yr( 0 ) );

	// write out the depresource objects.
	for( vector<SubResource*>::const_iterator i = subResource.begin(); i != subResource.end(); i++ ){
		( *i )->toInputXML( out, tabs );
	}

	// finished writing xml for the class members.
	XMLWriteClosingTag( getXMLName(), out, tabs );
}

//! Write datamembers to datastream in XML format for outputting results.
void Resource::toOutputXML( ostream& out, Tabs* tabs ) const {
	const Modeltime* modeltime = scenario->getModeltime();

	XMLWriteOpeningTag( getXMLName(), out, tabs, name );

	// write the xml for the class members.
	// write out the market string.
	XMLWriteElement( market, "market", out, tabs );

	// write out resource prices for all periods
	for(int m = 0; m < static_cast<int>(rscprc.size()); m++ ) {
		XMLWriteElement( rscprc[m], "price", out, tabs, modeltime->getper_to_yr(m));
	}

	// write out the depresource objects.
	for( vector<SubResource*>::const_iterator i = subResource.begin(); i != subResource.end(); i++ ){
		( *i )->toInputXML( out, tabs );
	}

	// finished writing xml for the class members.

	XMLWriteClosingTag( getXMLName(), out, tabs );
}

//! Write datamembers to datastream in XML format for debugging.
void Resource::toDebugXML( const int period, ostream& out, Tabs* tabs ) const {

	XMLWriteOpeningTag( getXMLName(), out, tabs, name );

	// Write the xml for the class members.
	// Write out the market string.
	XMLWriteElement( market, "market", out, tabs );

	// Write out resource prices for debugging period.
	XMLWriteElement( rscprc[ period ], "rscprc", out, tabs );

	// Write out available resources for debugging period.
	XMLWriteElement( available[ period ], "available", out, tabs );

	// Write out annualprod for debugging period.
	XMLWriteElement( annualprod[ period ], "annualprod", out, tabs );

	// Write out cumulative prod for debugging period.
	XMLWriteElement( cummprod[ period ], "cummprod", out, tabs );

	// Write out the number of sub-resources.
	XMLWriteElement( nosubrsrc, "nosubrsrc", out, tabs );

	// Write out the depresource objects.
	for( vector<SubResource*>::const_iterator i = subResource.begin(); i != subResource.end(); i++ ){
		( *i )->toDebugXML( period, out, tabs );
	}

	// finished writing xml for the class members.

	XMLWriteClosingTag( getXMLName(), out, tabs );
}


//! Create markets
void Resource::setMarket( const string& regionName ) {

	Marketplace* marketplace = scenario->getMarketplace();
	const Modeltime* modeltime = scenario->getModeltime();
	// name is resource name
	if ( marketplace->createMarket( regionName, market, name, IMarketType::NORMAL ) ) {
		marketplace->setPriceVector( name, regionName, rscprc );
		for( int per = 1; per < modeltime->getmaxper(); ++per ){
			marketplace->setMarketToSolve( name, regionName, per );
		}
	}
}

//! Return resource name.

const string& Resource::getName() const {
    return name;
}

//! Return resource price.
double Resource::getPrice(int per)
{
	return rscprc[per] ;
}

//! Calculate total resource supply for a period.
void Resource::calcSupply( const string& regionName, const GDP* gdp, const int period ){
	// This code is moved down from Region
	Marketplace* marketplace = scenario->getMarketplace();

	double price = marketplace->getPrice( name, regionName, period );
	double lastPeriodPrice;

	if ( period == 0 ) {
		lastPeriodPrice = price;
	}
	else {
		lastPeriodPrice = marketplace->getPrice( name, regionName, period - 1 );
	}

	// calculate annual supply
	annualsupply( regionName, period, gdp, price, lastPeriodPrice ); 
	// set market supply of resource
	marketplace->addToSupply( name, regionName, annualprod[ period ], period );
}

void Resource::cumulsupply(double prc,int per)
{	
	int i=0;
	cummprod[per]=0.0;

	rscprc[per] = prc;
	// sum cumulative production of each subsector
	for (i=0;i<nosubrsrc;i++) {
		subResource[i]->cumulsupply(prc,per);
		cummprod[per] += subResource[i]->getCumulProd(per);
	}
}

//! Calculate annual production
void Resource::annualsupply( const std::string& regionName, int per, const GDP* gdp, double price, double prev_price )
{	
	int i=0;
	annualprod[per]=0.0;
	available[per]=0.0;

	// calculate cummulative production
	cumulsupply(price,per);

	// sum annual production of each subsector
	for (i=0;i<nosubrsrc;i++) {
		subResource[i]->annualsupply( per, gdp, price, prev_price );
		annualprod[per] += subResource[i]->getAnnualProd(per);
		available[per] += subResource[i]->getAvailable(per);
	}
}


//! Return annual production of resources.
double Resource::getAnnualProd(int per) {
	return annualprod[per];
}

//! Write resource output to file.
void Resource::csvOutputFile( const string& regname )
{
	// function protocol
	void fileoutput3( string var1name,string var2name,string var3name,
		string var4name,string var5name,string uname,vector<double> dout);

	// function arguments are variable name, double array, db name, table name
	// the function writes all years
	// total sector output
	fileoutput3( regname,name," "," ","production","EJ",annualprod);

	// do for all subsectors in the sector
	for (int i=0;i<nosubrsrc;i++) {
		subResource[i]->csvOutputFile(regname ,name);
	}
}

//! Write resource output to database.
void Resource::dbOutput( const string& regname ) {
	const Modeltime* modeltime = scenario->getModeltime();
	const int maxper = modeltime->getmaxper();
	vector<double> temp(maxper);
	// function protocol
	void dboutput4(string var1name,string var2name,string var3name,string var4name,
		string uname,vector<double> dout);

	// function arguments are variable name, double array, db name, table name
	// the function writes all years
	// total sector output
	dboutput4(regname,"Pri Energy","Production by Sector",name,"EJ",annualprod);
	// resource price
	dboutput4(regname,"Price","by Sector",name,"$/GJ",rscprc);
	// do for all subsectors in the sector
	for (int m=0;m<maxper;m++) {
		for (int i=0;i<nosubrsrc;i++) {
			temp[m] += subResource[i]->getCumulProd(m);
		}
	}
	dboutput4(regname,"Resource","CummProd "+name,"zTotal","EJ",temp);

	// do for all subsectors in the sector
	for (int i=0;i<nosubrsrc;i++) {
		subResource[i]->dbOutput(regname,name);
	}
}

/*! \brief A function to add the resource sectors to an existing graph.
*
* For resource sectors only add style information.
*
* \author Josh Lurz, Steve Smith
* \param outStream An output stream to write to which was previously created.
* \param period The period to print graphs for.
*/
void Resource::addToDependencyGraph( ostream& outStream, const int period ) const {

	// Print out the style for the Sector.
	printStyle( outStream );
}


/*! \brief A function to add the Sector coloring and style to the dependency graph.
*
* This function add the Sector specific coloring and style to the dependency graph.
*
* \author Josh Lurz
* \param outStream An output stream to write to which was previously created.
*/
void Resource::printStyle( ostream& outStream ) const {

	// Make sure the output stream is open.
	assert( outStream );

	// Get the sector name.
	string sectorName = getName();
	util::replaceSpaces( sectorName );

	// output sector coloring here.
	outStream << "\t" << sectorName << " [shape=box, style=filled, color=indianred1 ];" << endl;
}

/*! \brief Set marketInfo for fixed supplies for this resources.
*
* For now this sets -1 to flag that supplies are not fixed.
* This will need to change once resource supply are calibrated.
* \author Steve Smith
* \param period model period.
*/
void Resource::setCalibratedSupplyInfo( const int period, const std::string& regionName ) {
	const double MKT_NOT_ALL_FIXED = -1;
	Marketplace* marketplace = scenario->getMarketplace();

	marketplace->getMarketInfo( name, regionName, period, true )->setDouble( "calSupply", MKT_NOT_ALL_FIXED );
}

// ************************************************************
// Definitions for the derived classes below.
// Since these are very small changes, keep in same file for simplicity
// ************************************************************

// *******************************************************************
// DepletableResource Class
// *******************************************************************
/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overridden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& DepletableResource::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& DepletableResource::getXMLNameStatic() {
	return XML_NAME;
}

//! 
/*! In this case, this read-in just substantiates the appropriate type of subResource */
/*! \brief Performs XML read-in that is specific to this derived class
*
*  this case, this read-in just instantiates the appropriate type of subResource
*
* \author Steve Smith
* \param node pointer to the current node in the XML input tree
* \param nodeName name of the current node 
* \return Whether an element was parsed.
* \todo In the input file, *SubResources should be read in as such, not silently converted here.
*/
bool DepletableResource::XMLDerivedClassParse( const string& nodeName, const DOMNode* node ) {
	if( nodeName == SubResource::getXMLNameStatic() || nodeName == SubDepletableResource::getXMLNameStatic() ){
		parseContainerNode( node, subResource, subResourceNameMap, new SubDepletableResource() );
		return true;
	}
	return false;
}

// *******************************************************************
// FixedResource Class
// *******************************************************************
/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overridden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& FixedResource::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& FixedResource::getXMLNameStatic() {
	return XML_NAME;
}

//! 
/*! In this case, this read-in just substantiates the appropriate type of subResource */
/*! \brief Performs XML read-in that is specific to this derived class
*
*  this case, this read-in just instantiates the appropriate type of subResource
*
* \author Steve Smith
* \param node pointer to the current node in the XML input tree
* \param nodeName name of the current node 
* \return Whether an element was parsed.
*/
bool FixedResource::XMLDerivedClassParse( const string& nodeName, const DOMNode* node ) {
	if( nodeName == SubResource::getXMLNameStatic() || nodeName == SubFixedResource::getXMLNameStatic() ){
		parseContainerNode( node, subResource, subResourceNameMap, new SubFixedResource() );
		return true;
	}
	return false;
}
// *******************************************************************
// RenewableResource Class
// *******************************************************************

//! \brief set the size for the resourceVariance member
RenewableResource::RenewableResource(){
	resourceVariance.resize( scenario->getModeltime()->getmaxper() );
	resourceCapacityFactor.resize( scenario->getModeltime()->getmaxper() );
}


/*! \brief Get the XML node name for output to XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* This function may be virtual to be overridden by derived class pointers.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME.
*/
const std::string& RenewableResource::getXMLName() const {
	return XML_NAME;
}

/*! \brief Get the XML node name in static form for comparison when parsing XML.
*
* This public function accesses the private constant string, XML_NAME.
* This way the tag is always consistent for both read-in and output and can be easily changed.
* The "==" operator that is used when parsing, required this second function to return static.
* \note A function cannot be static and virtual.
* \author Josh Lurz, James Blackwood
* \return The constant XML_NAME as a static.
*/
const std::string& RenewableResource::getXMLNameStatic() {
	return XML_NAME;
}

//! 
/*! In this case, this read-in just substantiates the appropriate type of subResource */
/*! \brief Performs XML read-in that is specific to this derived class
*
*  this case, this read-in just instantiates the appropriate type of subResource
*
* \author Steve Smith
* \param node pointer to the current node in the XML input tree
* \param nodeName name of the current node 
* \return Whether an element was parsed.
*/
bool RenewableResource::XMLDerivedClassParse( const string& nodeName, const DOMNode* node ) {
	if( nodeName == SubResource::getXMLNameStatic() || nodeName == SubRenewableResource::getXMLNameStatic() ){
		parseContainerNode( node, subResource, subResourceNameMap, new SubRenewableResource() );
		return true;
	}
	return false;
}

//! Calculate annual production
/*! \brief Adds to the base Resource::annualsupply by computing a weighted-average
*  variance of the resource based on the variance of the subresources.
*
* \author Steve Smith.  Mod for intermittent by Marshall Wise
*/
void RenewableResource::annualsupply( const std::string& regionName, int per, const GDP* gdp, double price, double prev_price )
{

	// calculate cummulative production
	cumulsupply(price,per);

    // clear out sums for this iteration
	annualprod[per]=0.0;
	available[per]=0.0;
	resourceVariance[per]=0.0;
	resourceCapacityFactor[per] = 0.0;

	// sum annual production of each subsector
	for (int i=0;i<nosubrsrc;i++) {
		subResource[i]->annualsupply( per, gdp, price, prev_price );
		annualprod[per] += subResource[i]->getAnnualProd(per);
		available[per] += subResource[i]->getAvailable(per);
		// and compute weighted average variance
		resourceVariance[per] += subResource[i]->getAnnualProd(per) * subResource[i]->getVariance();
		// and compute weighted average capacityfactor
		resourceCapacityFactor[per] += subResource[i]->getAnnualProd(per) * subResource[i]->getAverageCapacityFactor();
	}

	// check for annual production = 0  util::getsmallnumber
	if (annualprod[per] > util::getSmallNumber()) {
		resourceVariance[per] /= annualprod[per];
		resourceCapacityFactor[per] /= annualprod[per];
	}

	// add variance to marketinfo
	IInfo* marketInfo = scenario->getMarketplace()->getMarketInfo( name, regionName, per, true );
	marketInfo->setDouble( "resourceVariance", resourceVariance[ per ] );

	// add capacity factor to marketinfo
	marketInfo->setDouble( "resourceCapacityFactor", resourceCapacityFactor[ per ] );
}
