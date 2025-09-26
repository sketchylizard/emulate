#include "apple2/disk_controller.h"
#include "catch2/catch_test_macros.hpp"
#include "disk_controller_helper.h"

using namespace apple2;

TEST_CASE("DiskController.seek track zero", "[apple2][disk_controller]")
{
  apple2::DiskController dc;
  CHECK_FALSE(dc.isMotorOn());

  DiskControllerHelper helper{dc};
  helper.seekTrack0();
  CHECK(dc.getCurrentTrack() != 0);

  helper.motorOn();
  CHECK(dc.isMotorOn());
  helper.seekTrack0();

  CHECK(dc.getCurrentTrack() == 0);

  helper.seekTrack(35);
  CHECK(dc.getCurrentTrack() == 34);
  helper.motorOff();

  CHECK_FALSE(dc.isMotorOn());
}
