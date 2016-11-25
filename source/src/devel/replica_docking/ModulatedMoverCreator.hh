/// @authoer Zhe Zhang
#ifndef INCLUDED_devel_replica_docking_ModulatedMoverCreator_hh
#define INCLUDED_devel_replica_docking_ModulatedMoverCreator_hh

#include <protocols/moves/MoverCreator.hh>

namespace devel {
namespace replica_docking {

class ModulatedMoverCreator : public protocols::moves::MoverCreator {
public:
	// XRW TEMP  protocols::moves::MoverOP create_mover() const override;
	// XRW TEMP  std::string keyname() const override;
	// XRW TEMP  static std::string mover_name();
	protocols::moves::MoverOP create_mover() const override;
	std::string keyname() const override;
	void provide_xml_schema( utility::tag::XMLSchemaDefinition & xsd ) const override;
};

}
}
#endif  // INCLUDED_devel_replica_docking_ModulatedMoverCreator_hh
