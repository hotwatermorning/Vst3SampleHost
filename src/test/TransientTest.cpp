#include "catch2/catch.hpp"

#include "../misc/Transient.hpp"

TEST_CASE("Transient test", "[transient]")
{
    using namespace hwm;
    
    Transient tr;
    tr = Transient(96000, 1000, -10, 10);
    
    REQUIRE(tr.get_max_db() == 10);
    REQUIRE(tr.get_min_db() == -10);
    REQUIRE(tr.get_target_db() == 0);
    REQUIRE(tr.get_current_transient_db() == 0);
    REQUIRE(tr.get_current_transient_linear_gain() == 1.0);
    
    tr.set_target_db(1000);
    REQUIRE(tr.get_target_db() == tr.get_max_db());
    
    tr.set_target_db(-1000);
    REQUIRE(tr.get_target_db() == tr.get_min_db());
    
    tr.set_target_db(-5);
    REQUIRE(tr.get_target_db() == -5);
    REQUIRE(tr.get_current_transient_db() == 0);
    
    // 1サンプルでどれだけdB値が変化するか
    auto const transient_per_step = log10(2) * 20 / (1000 / 1000 * 96000);
    auto const kTolerance = 0.000001;
    
    auto const cur = tr.get_current_transient_db();
    
    tr.update_transient(1);
    REQUIRE(fabs(tr.get_current_transient_db() - (cur - transient_per_step * 1)) < kTolerance);
    tr.update_transient(10);
    REQUIRE(fabs(tr.get_current_transient_db() - (cur - transient_per_step * 11)) < kTolerance);
    tr.update_transient(100);
    REQUIRE(fabs(tr.get_current_transient_db() - (cur - transient_per_step * 111)) < kTolerance);
    
    tr.update_transient(96000 * 4);
    REQUIRE(std::fabs(tr.get_current_transient_db() - tr.get_target_db()) < kTolerance);
    REQUIRE(tr.get_current_transient_linear_gain() != 0);
    
    tr.set_target_db(2.5);
    tr.update_transient(96000 * 4);
    REQUIRE(std::fabs(tr.get_current_transient_db() - tr.get_target_db()) < kTolerance);
    REQUIRE(tr.get_current_transient_linear_gain() != 0);
    
    tr.set_target_db(-100);
    tr.update_transient(96000 * 4);
    REQUIRE(std::fabs(tr.get_current_transient_db() - tr.get_min_db()) < kTolerance);
    REQUIRE(tr.get_current_transient_linear_gain() == 0);
    
    tr.set_target_db(100);
    tr.update_transient(96000 * 4);
    REQUIRE(std::fabs(tr.get_current_transient_db() - tr.get_max_db()) < kTolerance);
    REQUIRE(tr.get_current_transient_linear_gain() != 0);
}
