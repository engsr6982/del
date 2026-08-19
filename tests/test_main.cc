#include "del/ast.h"
#include "del/context.h"
#include "del/exception.h"
#include "del/lexer.h"
#include "del/parser.h"
#include "del/symbol_table.h"
#include "del/template_engine.h"
#include "del/version.h"
#include <iostream>

int main(int argc, char** argv) {
  del::TemplateEngine engine;


  std::cout << "\n==================================" << std::endl;

  nlohmann::json source_1 = nlohmann::json::parse(R"(
    {
      "player_permissions": {
        "4A2E-8F1A-99BC": 1,
        "1111-2222-3333": 2
      }
    }
  )");

  nlohmann::ordered_json template_1 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/mPermissions": "map_object(@/player_permissions, (key, val) -> entry(key |> replace('-', ''), val == 1 ? \"admin\" : \"member\") ) "
    }
  )");

  try {
    auto           compiled_t1 = engine.Compile(template_1);
    nlohmann::json result_1    = engine.Execute(compiled_t1, source_1);
    std::cout << result_1.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;

  nlohmann::json source_2 = nlohmann::json::parse(R"(
    {
      "legacy_meta": {
        "uuid": "4A2E-8F1A-99BC",
        "display": "  My Land-- ",
        "status": 1
      },
      "member_list": ["aaa-111", "bbb-222"]
    }
  )");

  nlohmann::ordered_json template_2 = nlohmann::ordered_json::parse(R"(
    {
      "/version": 32,
      "/land_info/owner_id": "@/legacy_meta/uuid |> replace('-', '') |> to_lower() ",
      "/land_info/clean_name": "@/legacy_meta/display |> trim() |> remove_suffix('-') ",
      "/land_info/hold_type": "@/legacy_meta/status == 1 ? \"Bought\" : \"Leasing\"",
      "/flags/is_valid": "is_null($/land_info/owner_id) ? false : true",
      "/flags/need_repair": "$/flags/is_valid == false || $/land_info/clean_name == \"\" ? true : false",
      "/members": "map(@/member_list, user -> user |> replace('-', '') |> to_lower()) ",
      "/fallback_test": "@/non_existent_field ?? \"default_value\""
    }
  )");

  try {
    auto           compiled_t2 = engine.Compile(template_2);
    nlohmann::json result_2    = engine.Execute(compiled_t2, source_2);
    std::cout << result_2.dump(2) << std::endl;
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;

  nlohmann::ordered_json error_template = nlohmann::ordered_json::parse(R"(
    {
      "/error_case": "true == \"not_boolean\""
    }
  )");

  try {
    auto compiled_err = engine.Compile(error_template);
    engine.Execute(compiled_err, source_2);
  } catch (const std::exception& e) {
    std::cout << "Expected exception caught successfully:\n" << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;
  std::cout << "get / at 内置函数单元测试" << std::endl;

  nlohmann::json source_get = nlohmann::json::parse(R"(
    {
      "land": {
        "range": { "start_position": [100, 60, 100], "dimid": 0 },
        "name": "my home"
      }
    }
  )");

  nlohmann::ordered_json tpl_get = nlohmann::ordered_json::parse(R"JSON(
    {
      "/t/get_exist":     "get(@/land, 'name')",
      "/t/get_missing":   "get(@/land, 'nope')",
      "/t/get_null_prop": "get(get(@/land, 'nope'), 'x')",
      "/t/at_exist":      "at(get(@/land/range, 'start_position'), 1)",
      "/t/at_oob":        "at(get(@/land/range, 'start_position'), 99)",
      "/t/at_negative":   "at(get(@/land/range, 'start_position'), -1)",
      "/t/at_null_prop":  "at(get(@/land, 'nope'), 0)",
      "/t/nested_chain":  "get(get(@/land, 'range'), 'dimid') ?? -1"
    }
  )JSON");

  try {
    auto           compiled_get = engine.Compile(tpl_get);
    nlohmann::json result_get   = engine.Execute(compiled_get, source_get);
    std::cout << result_get.dump(2) << std::endl;

    auto check = [](bool ok, std::string_view name) {
      std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    };
    check(result_get["t"]["get_exist"] == "my home", "get 命中键");
    check(result_get["t"]["get_missing"].is_null(), "get 缺失键 -> null");
    check(result_get["t"]["get_null_prop"].is_null(), "get 对 null 容器 null 传播");
    check(result_get["t"]["at_exist"] == 60, "at 命中下标");
    check(result_get["t"]["at_oob"].is_null(), "at 越界 -> null");
    check(result_get["t"]["at_negative"].is_null(), "at 负下标 -> null");
    check(result_get["t"]["at_null_prop"].is_null(), "at 对 null 容器 null 传播");
    check(result_get["t"]["nested_chain"] == 0, "get 链 + ?? 兜底");
  } catch (const std::exception& e) {
    std::cerr << e.what() << std::endl;
  }

  std::cout << "\n==================================" << std::endl;
  std::cout << "get / at 类型错误用例" << std::endl;

  {
    nlohmann::ordered_json tpl_err = nlohmann::ordered_json::parse(R"JSON(
      {
        "/e/get_on_number": "get(42, 'x')",
        "/e/at_on_object":  "at(@/land/range, 0)",
        "/e/at_float_idx":  "at(@/land/range/start_position, 1.5)"
      }
    )JSON");
    auto            compiled_err2 = engine.Compile(tpl_err);
    nlohmann::json  dummy_target;
    auto            ctx_err = engine.CreateContext(source_get, dummy_target);
    for (auto& instr : compiled_err2.instructions) {
      try {
        engine.Execute(instr, ctx_err);
        std::cout << "[FAIL] 期望 RuntimeError 但未抛出: " << instr.target_pointer_path << std::endl;
      } catch (del::RuntimeError const& e) {
        std::cout << "[PASS] 正确抛出: " << instr.target_pointer_path << " -> " << e.what() << std::endl;
      }
    }
  }

  std::cout << "\n==================================" << std::endl;
  std::cout << "iLand -> PLand 双输入联动转换（get/at + 嵌套 lambda JOIN + 无主过滤）" << std::endl;

  {
    // 模拟宿主注册的 XUID -> UUID 服务（真实环境由 PLand/PlayerInfo 提供，
    // 与 REPL 的 set() 一样通过 RegisterCustomFunction 注入）
    engine.RegisterCustomFunction(
        "xuid_to_uuid",
        [](del::Arguments const& args, del::EvaluationContext& ctx, del::ExprEvaluator const& eval) -> nlohmann::json {
          auto v = eval(*args[0], ctx);
          if (!v.is_string()) return nullptr;
          auto const& xuid = v.get<std::string>();
          if (xuid == "1234567890abcdef") return nlohmann::json("82c5615f-92be-32b1-83c8-b937284d9de6");
          return nullptr;
        });

    // 双文件在调用方合并为单一 source（引擎为单源设计）
    // land_id_004 为无主领地（不在任何 Owner 列表中），应被过滤
    // land_id_003 角点故意翻转，用于验证 mPos 逐轴 min/max 归一化
    nlohmann::json merged_src = nlohmann::json::parse(R"JSON(
      {
        "relationship": {
          "version": 284,
          "Owner": {
            "1234567890abcdef": ["land_id_001", "land_id_002"],
            "fedcba0987654321": ["land_id_003"]
          }
        },
        "data": {
          "version": 284,
          "Lands": {
            "land_id_001": {
              "range": { "start_position": [100, 60, 100], "end_position": [120, 90, 120], "dimid": 0 },
              "settings": { "nickname": "我的温馨小屋", "teleport": [110, 65, 110], "ev_farmland_decay": false, "ev_explode": false, "ev_piston_push": false, "ev_fire_spread": false, "ev_redstone_update": true },
              "permissions": { "use_dispenser": true, "use_door": true, "allow_dropitem": true, "allow_pickupitem": true, "allow_place": false, "use_fence_gate": true, "use_pressure_plate": true, "use_firegen": false, "use_campfire": true, "use_furnace": true, "allow_throw_potion": true, "use_beacon": true, "use_daylight_detector": true, "allow_attack_player": false, "use_lectern": true, "use_fishing_hook": true, "use_lever": true, "use_button": true, "allow_attack_mobs": true, "use_composter": true, "allow_ride_entity": true, "use_noteblock": true, "use_bucket": true, "allow_destroy": false, "use_respawn_anchor": true, "use_jukebox": true, "use_bed": true, "use_item_frame": true, "use_trapdoor": true, "use_armor_stand": true, "allow_ride_trans": true, "use_cauldron": true, "use_bell": true }
            },
            "land_id_002": {
              "range": { "start_position": [200, 64, 200], "end_position": [230, 100, 230], "dimid": 0 },
              "settings": { "nickname": "二号领地", "teleport": [215, 70, 215], "ev_farmland_decay": true, "ev_explode": true, "ev_piston_push": false, "ev_fire_spread": true, "ev_redstone_update": false },
              "permissions": { "use_door": false, "allow_dropitem": false, "allow_place": true, "use_firegen": true, "use_furnace": false, "allow_attack_player": true, "allow_attack_mobs": false, "use_bed": false, "allow_destroy": true, "allow_ride_entity": false }
            },
            "land_id_003": {
              "range": { "start_position": [340, 110, 340], "end_position": [300, 70, 300], "dimid": 1 },
              "settings": { "nickname": "三号领地", "teleport": [320, 80, 320], "ev_farmland_decay": false, "ev_explode": false, "ev_piston_push": true, "ev_fire_spread": false, "ev_redstone_update": true },
              "permissions": { "use_door": true, "allow_dropitem": true, "allow_place": false, "use_firegen": false, "use_furnace": true, "allow_attack_player": false, "allow_attack_mobs": true, "use_bed": true, "allow_destroy": false, "allow_ride_entity": true }
            },
            "land_id_004": {
              "range": { "start_position": [0, 0, 0], "end_position": [10, 10, 10], "dimid": 0 },
              "settings": { "nickname": "无主领地" },
              "permissions": {}
            }
          }
        }
      }
    )JSON");

    // 与 examples/iland_to_pland/template.json 保持同步
    nlohmann::ordered_json tpl_convert = nlohmann::ordered_json::parse(R"JSON(
      {
        "/records": "map_object(@/data/Lands, (land_id, land) -> is_null(reduce(values(map_object(@/relationship/Owner, (owner, lands) -> entry(owner, contains(lands, land_id) ? owner : null))), null, (acc, v) -> is_null(v) ? acc : v)) ? entry('_skip', null) : entry(land_id, entry('mHoldType', 'Bought') |> put('mIs3DLand', true) |> put('mIsConvertedLand', true) |> put('mLandDimid', get(get(land, 'range'), 'dimid') ?? 0) |> put('mLandMembers', array()) |> put('mLandName', get(get(land, 'settings'), 'nickname') ?? 'Unnamed territories') |> put('mLandOwner', xuid_to_uuid(reduce(values(map_object(@/relationship/Owner, (owner, lands) -> entry(owner, contains(lands, land_id) ? owner : null))), null, (acc, v) -> is_null(v) ? acc : v)) ?? reduce(values(map_object(@/relationship/Owner, (owner, lands) -> entry(owner, contains(lands, land_id) ? owner : null))), null, (acc, v) -> is_null(v) ? acc : v)) |> put('mLandPermTable', entry('environment', entry('allowAnimalSpawn', true) |> put('allowBlockFall', false) |> put('allowDragonEggTeleport', false) |> put('allowExplode', get(get(land, 'settings'), 'ev_explode') ?? false) |> put('allowFarmDecay', get(get(land, 'settings'), 'ev_farmland_decay') ?? false) |> put('allowFireSpread', get(get(land, 'settings'), 'ev_fire_spread') ?? false) |> put('allowLightningBolt', true) |> put('allowLiquidFlow', true) |> put('allowMinecartHopperPullItems', true) |> put('allowMobGrief', false) |> put('allowMonsterSpawn', true) |> put('allowMossGrowth', true) |> put('allowPistonPushOnBoundary', get(get(land, 'settings'), 'ev_piston_push') ?? false) |> put('allowRedstoneUpdate', get(get(land, 'settings'), 'ev_redstone_update') ?? false) |> put('allowSculkBlockGrowth', true) |> put('allowSculkSpread', false) |> put('allowWitherDestroy', false)) |> put('role', entry('allowDestroy', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_destroy') ?? false)) |> put('allowDropItem', entry('actor', true) |> put('member', get(get(land, 'permissions'), 'allow_dropitem') ?? false)) |> put('allowFishingRodAndHook', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_fishing_hook') ?? false)) |> put('allowFriendlyDamage', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_attack_player') ?? false)) |> put('allowHostileDamage', entry('actor', true) |> put('member', get(get(land, 'permissions'), 'allow_attack_mobs') ?? false)) |> put('allowInteractEntity', entry('actor', false) |> put('member', true)) |> put('allowPlace', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_place') ?? false)) |> put('allowPlayerPickupItem', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_pickupitem') ?? false)) |> put('allowPvP', entry('actor', false) |> put('member', false)) |> put('allowRideEntity', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_ride_entity') ?? false)) |> put('allowRideTrans', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_ride_trans') ?? false)) |> put('allowSpecialEntityDamage', entry('actor', false) |> put('member', false)) |> put('allowTriggerDripleaf', entry('actor', true) |> put('member', true)) |> put('allowUseRangedWeapon', entry('actor', false) |> put('member', true)) |> put('allowUseThrowable', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'allow_throw_potion') ?? false)) |> put('editFlowerPot', entry('actor', false) |> put('member', true)) |> put('editSign', entry('actor', false) |> put('member', true)) |> put('placeBoat', entry('actor', false) |> put('member', true)) |> put('placeMinecart', entry('actor', false) |> put('member', true)) |> put('useArmorStand', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_armor_stand') ?? false)) |> put('useAxe', entry('actor', false) |> put('member', true)) |> put('useBeacon', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_beacon') ?? false)) |> put('useBed', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_bed') ?? false)) |> put('useBeeNest', entry('actor', false) |> put('member', true)) |> put('useBell', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_bell') ?? false)) |> put('useBoneMeal', entry('actor', false) |> put('member', true)) |> put('useBucket', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_bucket') ?? false)) |> put('useButton', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_button') ?? false)) |> put('useCake', entry('actor', false) |> put('member', true)) |> put('useCampfire', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_campfire') ?? false)) |> put('useCauldron', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_cauldron') ?? false)) |> put('useComparator', entry('actor', false) |> put('member', true)) |> put('useComposter', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_composter') ?? false)) |> put('useContainer', entry('actor', false) |> put('member', true)) |> put('useDaylightDetector', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_daylight_detector') ?? false)) |> put('useDoor', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_door') ?? false)) |> put('useFenceGate', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_fence_gate') ?? false)) |> put('useFlintAndSteel', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_firegen') ?? false)) |> put('useFurnaces', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_furnace') ?? false)) |> put('useHoe', entry('actor', false) |> put('member', true)) |> put('useItemFrame', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_item_frame') ?? false)) |> put('useJukebox', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_jukebox') ?? false)) |> put('useLectern', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_lectern') ?? false)) |> put('useLever', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_lever') ?? false)) |> put('useNoteBlock', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_noteblock') ?? false)) |> put('usePressurePlate', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_pressure_plate') ?? false)) |> put('useRepeater', entry('actor', false) |> put('member', true)) |> put('useRespawnAnchor', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_respawn_anchor') ?? false)) |> put('useShovel', entry('actor', false) |> put('member', true)) |> put('useTrapdoor', entry('actor', false) |> put('member', get(get(land, 'permissions'), 'use_trapdoor') ?? false)) |> put('useWorkstation', entry('actor', false) |> put('member', true)))) |> put('mLeasing', entry('mEndAt', 0) |> put('mStartAt', 0) |> put('mState', 'None')) |> put('mOriginalBuyPrice', 0) |> put('mOwnerDataIsXUID', is_null(xuid_to_uuid(reduce(values(map_object(@/relationship/Owner, (owner, lands) -> entry(owner, contains(lands, land_id) ? owner : null))), null, (acc, v) -> is_null(v) ? acc : v)))) |> put('mParentLandID', -1) |> put('mPos', entry('max', entry('x', (at(get(get(land, 'range'), 'start_position'), 0) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 0) ?? 0) ? at(get(get(land, 'range'), 'end_position'), 0) ?? 0 : at(get(get(land, 'range'), 'start_position'), 0) ?? 0) |> put('y', (at(get(get(land, 'range'), 'start_position'), 1) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 1) ?? 0) ? at(get(get(land, 'range'), 'end_position'), 1) ?? 0 : at(get(get(land, 'range'), 'start_position'), 1) ?? 0) |> put('z', (at(get(get(land, 'range'), 'start_position'), 2) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 2) ?? 0) ? at(get(get(land, 'range'), 'end_position'), 2) ?? 0 : at(get(get(land, 'range'), 'start_position'), 2) ?? 0)) |> put('min', entry('x', (at(get(get(land, 'range'), 'start_position'), 0) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 0) ?? 0) ? at(get(get(land, 'range'), 'start_position'), 0) ?? 0 : at(get(get(land, 'range'), 'end_position'), 0) ?? 0) |> put('y', (at(get(get(land, 'range'), 'start_position'), 1) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 1) ?? 0) ? at(get(get(land, 'range'), 'start_position'), 1) ?? 0 : at(get(get(land, 'range'), 'end_position'), 1) ?? 0) |> put('z', (at(get(get(land, 'range'), 'start_position'), 2) ?? 0) < (at(get(get(land, 'range'), 'end_position'), 2) ?? 0) ? at(get(get(land, 'range'), 'start_position'), 2) ?? 0 : at(get(get(land, 'range'), 'end_position'), 2) ?? 0))) |> put('mSubLandIDs', array()) |> put('mTeleportPos', entry('x', at(get(get(land, 'settings'), 'teleport'), 0) ?? 0) |> put('y', at(get(get(land, 'settings'), 'teleport'), 1) ?? 0) |> put('z', at(get(get(land, 'settings'), 'teleport'), 2) ?? 0)))) |> values() |> filter((rec) -> !is_null(rec)) |> map((rec, idx) -> put(put(rec, 'mLandID', idx + 1), 'version', 32))"
      }
    )JSON");

    try {
    auto           compiled_conv = engine.Compile(tpl_convert);
    nlohmann::json result_conv   = engine.Execute(compiled_conv, merged_src);
    std::cout << result_conv.dump(2) << std::endl;

    auto  check = [](bool ok, std::string_view name) {
      std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
    };
    auto& records = result_conv["records"];

    check(records.is_array() && records.size() == 3, "3 块有主领地 -> 3 条独立记录（无主 land_id_004 被过滤）");

    if (records.is_array() && records.size() == 3) {
      auto& r0 = records[0]; // land_id_001
      check(r0["mLandID"] == 1 && r0["version"] == 32, "mLandID 序号 + version");
      check(r0["mHoldType"] == "Bought" && r0["mIs3DLand"] == true && r0["mIsConvertedLand"] == true, "静态字段");
      check(r0["mLandName"] == "我的温馨小屋" && r0["mLandDimid"] == 0, "name/dimid 从绑定值提取");
      check(
          r0["mLandOwner"] == "82c5615f-92be-32b1-83c8-b937284d9de6" && r0["mOwnerDataIsXUID"] == false,
          "XUID -> UUID 自定义函数"
      );
      check(r0["mPos"]["min"] == nlohmann::json{{"x", 100}, {"y", 60}, {"z", 100}}, "mPos.min");
      check(r0["mPos"]["max"] == nlohmann::json{{"x", 120}, {"y", 90}, {"z", 120}}, "mPos.max");
      check(r0["mTeleportPos"] == nlohmann::json{{"x", 110}, {"y", 65}, {"z", 110}}, "mTeleportPos");
      check(r0["mLandPermTable"]["environment"]["allowRedstoneUpdate"] == true, "env 源驱动 ev_redstone_update");
      check(r0["mLandPermTable"]["environment"]["allowExplode"] == false, "env 源驱动 ev_explode");
      check(r0["mLandPermTable"]["role"]["useDoor"]["member"] == true, "role useDoor.member <- use_door");
      check(
          r0["mLandPermTable"]["role"]["allowDropItem"] == nlohmann::json{{"actor", true}, {"member", true}},
          "role allowDropItem 例外 actor=true"
      );
      check(
          r0["mLandPermTable"]["role"]["allowFriendlyDamage"] == nlohmann::json{{"actor", false}, {"member", false}},
          "role allowFriendlyDamage <- allow_attack_player"
      );

      auto& r1 = records[1]; // land_id_002（精简权限表演示 ?? false 兜底）
      check(
          r1["mLandID"] == 2 && r1["mLandOwner"] == "82c5615f-92be-32b1-83c8-b937284d9de6",
          "land_id_002 归属 XUID -> UUID"
      );
      check(r1["mLandPermTable"]["environment"]["allowFarmDecay"] == true, "env allowFarmDecay <- ev_farmland_decay");
      check(r1["mLandPermTable"]["role"]["useDoor"]["member"] == false, "role useDoor.member <- use_door(false)");
      check(r1["mLandPermTable"]["role"]["useBeacon"]["member"] == false, "缺失权限键 ?? false 兜底");

      auto& r2 = records[2]; // land_id_003（翻转角点归一化 + UUID 缺失回退 XUID）
      check(
          r2["mLandID"] == 3 && r2["mLandOwner"] == "fedcba0987654321" && r2["mOwnerDataIsXUID"] == true,
          "UUID 缺失回退 XUID"
      );
      check(
          r2["mPos"]["min"] == nlohmann::json{{"x", 300}, {"y", 70}, {"z", 300}}
              && r2["mPos"]["max"] == nlohmann::json{{"x", 340}, {"y", 110}, {"z", 340}},
          "翻转角点逐轴 min/max 归一化"
      );
    }
    } catch (const std::exception& e) {
      std::cerr << e.what() << std::endl;
    }
  }

  return 0;
}