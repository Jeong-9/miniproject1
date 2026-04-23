#define _CRT_SECURE_NO_WARNINGS
#define NOMINMAX

#include "common_types.h"
#include "map_system.h"
#include "sim_engine.h"
#include "renderer.h"
#include "course_evaluator.h"
#include "traffic_system.h"
#include "replay_recorder.h"
#include "replay_player.h"
#include "parking_evaluator.h"

#include "exam_result.h"

#include <SFML/System/Clock.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/Window/Keyboard.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <string>
#include <vector>

using namespace bestdriver;

namespace {

    enum class AppScreen {
        Menu,
        RoadDriving,
        Parking,
        Collision,
        CollisionDetail,// renderer.h 파라미터 부분에서 가져온 부분
        Result,   // renderer.h 파라미터 부분에서 가져온 부분
        ResultDetail,   // renderer.h 파라미터 부분에서 가져온 부분
        Replay
    };
    
    struct PrevKeys {
        bool e, g, a, d, z, x, r, q, v, b, esc, space, left, right;
        PrevKeys()
            : e(false), g(false), a(false), d(false), z(false), x(false), r(false), q(false),
            v(false), b(false), esc(false), space(false), left(false), right(false) {
        }
    };


    struct MenuNavState {
        int selectedIndex;
        bool prevUp;
        bool prevDown;
        bool prevActivate;
        bool activatePressed;
        MenuNavState()
            : selectedIndex(0), prevUp(false), prevDown(false), prevActivate(false), activatePressed(false) {
        }
    };

    struct KeySnapshot {
        bool w, s, left, right, e, g, a, d, z, x, r, q, v, b, esc, space;
        KeySnapshot()
            : w(false), s(false), left(false), right(false), e(false), g(false), a(false), d(false),
            z(false), x(false), r(false), q(false), v(false), b(false), esc(false), space(false) {
        }
    };

    struct InputResult {
        float dirX;
        float dirY;
        bool accel;
        bool brake;
        bool quit;
        bool restart;
        InputResult() : dirX(0.0f), dirY(0.0f), accel(false), brake(false), quit(false), restart(false) {}
    };

    enum class MapType {
        CourseA,
        CourseB,
        ParallelParking,
        TParking
    };

    struct Session {
        GameMode mode;
        MapType mapType;
        MapSystem map;
        SimEngine engine;
        TrafficSystem traffic;
        CourseEvaluator evaluator;
        ReplayRecorder recorder;
        ParkingEvaluator parkingEvaluator;
        ReplayPlayer replayPlayer;

        Trail trail;

        std::string warning;
        float warningTime;
        float elapsed;
        float parkSuccessTimer;
        int penalty;
        int cpDone;
        int cpTotal;
        bool parkOk;
        bool crashed;
        bool disqualified;

        ParkingSlot targetSlot;
        std::vector<TrafficSignal> finalSignals;
        std::vector< std::pair<Vec2, Penalty> > penaltyMarkers;
        float replaySpeed;
        bool replayPaused;

        Session()
            : mode(GameMode::RoadDriving),
            mapType(MapType::CourseA),
            warningTime(0.0f),
            elapsed(0.0f),
            penalty(0),
            cpDone(0),
            cpTotal(0),
            parkOk(false),
            crashed(false),
            disqualified(false),
            replaySpeed(1.0f),
            replayPaused(false),
            parkSuccessTimer(0.0f) {
        }
    };

    MenuNavState captureMenuNavState(const MenuNavState& previous) {
        MenuNavState state = previous;
        const bool up = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
        const bool down = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
        const bool activate = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter) ||
            sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);

        state.activatePressed = activate && !state.prevActivate;

        if (up && !state.prevUp) state.selectedIndex = (state.selectedIndex + 4) % 5;
        if (down && !state.prevDown) state.selectedIndex = (state.selectedIndex + 1) % 5;

        state.prevUp = up;
        state.prevDown = down;
        state.prevActivate = activate;
        return state;
    }

    KeySnapshot captureKeys(bool allowDrivingInput) {
        KeySnapshot ks;
        ks.esc = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape);
        ks.q = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Q);
        ks.r = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::R);
        ks.space = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space);
        ks.left = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Left);
        ks.right = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Right);
        ks.v = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::V);

        if (allowDrivingInput) {
            ks.w = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W);
            ks.s = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S);
            ks.e = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::E);
            ks.g = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::G);
            ks.a = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::A);
            ks.d = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::D);
            ks.z = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Z);
            ks.x = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::X);
            ks.b = sf::Keyboard::isKeyPressed(sf::Keyboard::Key::B);
        }

        return ks;
    }

    void initPrevKeys(PrevKeys& prev, const KeySnapshot& ks) {
        prev.e = ks.e;
        prev.g = ks.g;
        prev.a = ks.a;
        prev.d = ks.d;
        prev.z = ks.z;
        prev.x = ks.x;
        prev.r = ks.r;
        prev.q = ks.q;
        prev.v = ks.v;
        prev.b = ks.b;
        prev.esc = ks.esc;
        prev.space = ks.space;
        prev.left = ks.left;
        prev.right = ks.right;
    }

    InputResult processInput(VehicleState& v,
        std::string& warn,
        float& warnT,
        PrevKeys& prev,
        bool allowDrivingInput) {
        InputResult inp;
        const KeySnapshot ks = captureKeys(allowDrivingInput);

        if (allowDrivingInput) {
            if (ks.right) inp.dirX += 1.0f;
            if (ks.left)  inp.dirX -= 1.0f;
            inp.accel = ks.w;
            inp.brake = ks.s;

            if (ks.b && !prev.b) {
                v.seatbeltOn = !v.seatbeltOn;
                warn = v.seatbeltOn ? "SEATBELT ON" : "SEATBELT OFF!";
                warnT = 1.5f;
            }

            if (ks.e && !prev.e) {
                v.engineOn = !v.engineOn;
                if (!v.engineOn) v.speed = 0.0f;
            }

            if (ks.g && !prev.g) {
                if (std::abs(v.speed) < 0.5f) {
                    if (v.gear == Gear::P) v.gear = Gear::R;
                    else if (v.gear == Gear::R) v.gear = Gear::N;
                    else if (v.gear == Gear::N) v.gear = Gear::D;
                    else v.gear = Gear::P;
                }
                else {
                    warn = "STOP FIRST!";
                    warnT = 1.5f;
                }
            }

            if (ks.a && !prev.a) v.signal = (v.signal == Signal::Left) ? Signal::Off : Signal::Left;
            if (ks.d && !prev.d) v.signal = (v.signal == Signal::Right) ? Signal::Off : Signal::Right;
            if (ks.z && !prev.z) v.signal = (v.signal == Signal::Hazard) ? Signal::Off : Signal::Hazard;
            if (ks.x && !prev.x) v.autoHold = !v.autoHold;
        }

        if (ks.r && !prev.r) inp.restart = true;
        if ((ks.q && !prev.q) || (ks.esc && !prev.esc)) inp.quit = true;

        prev.e = ks.e;
        prev.g = ks.g;
        prev.a = ks.a;
        prev.d = ks.d;
        prev.z = ks.z;
        prev.x = ks.x;
        prev.r = ks.r;
        prev.q = ks.q;
        prev.v = ks.v;
        prev.b = ks.b;
        prev.esc = ks.esc;
        prev.space = ks.space;
        prev.left = ks.left;
        prev.right = ks.right;

        return inp;
    }

    VehicleControl buildControl(const InputResult& inp, const VehicleState& v) {
        VehicleControl c;
        c.accel = inp.accel ? 1.0f : 0.0f;
        c.brake = inp.brake ? 1.0f : 0.0f;
        c.dirX = inp.dirX;
        c.dirY = inp.dirY;
        c.steering = 0.0f;
        c.gear = v.gear;
        c.signal = v.signal;
        c.engineOn = v.engineOn;
        c.autoHold = v.autoHold;
        c.seatbeltOn = v.seatbeltOn;
        return c;
    }

    // ----------------------------------------------------------------
    // Traffic signal setup - generic helper
    // Places 4 signals per intersection (right, left, down, up)
    // vx[]: vertical road X origins, numV count
    // hy[]: horizontal road Y origins, numH count
    // RW: road width (5)
    // ----------------------------------------------------------------
    void buildTrafficSignals(
        TrafficSystem& traffic,
        const float vx[], int numV,
        const float hy[], int numH,
        float RW)
    {
        std::vector<TrafficSignal> signals;
        std::vector<TrafficSignalGroup> groups;

        const float signalOffset = 0.55f;

        const float GREEN_SEC = 22.0f;
        const float YELLOW_SEC = 3.0f;
        const float ALL_RED_SEC = 2.0f;

        auto setupHorizontalGroup = [&](TrafficSignalGroup& g) {
            g.phaseSequence.clear();
            g.phaseDurations.clear();
            g.phaseSequence.push_back(SignalPhase::MainGreen);
            g.phaseDurations.push_back(GREEN_SEC);
            g.phaseSequence.push_back(SignalPhase::MainYellow);
            g.phaseDurations.push_back(YELLOW_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(ALL_RED_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(GREEN_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(YELLOW_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(ALL_RED_SEC);
            g.currentPhaseIndex = 0;
            g.phaseTimer = g.phaseDurations[0];
        };

        auto setupVerticalGroup = [&](TrafficSignalGroup& g) {
            g.phaseSequence.clear();
            g.phaseDurations.clear();
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(GREEN_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(YELLOW_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(ALL_RED_SEC);
            g.phaseSequence.push_back(SignalPhase::MainGreen);
            g.phaseDurations.push_back(GREEN_SEC);
            g.phaseSequence.push_back(SignalPhase::MainYellow);
            g.phaseDurations.push_back(YELLOW_SEC);
            g.phaseSequence.push_back(SignalPhase::AllRed);
            g.phaseDurations.push_back(ALL_RED_SEC);
            g.currentPhaseIndex = 0;
            g.phaseTimer = g.phaseDurations[0];
        };

        auto applyOffsetToGroup = [&](TrafficSignalGroup& g, float offsetSec) {
            float cycle = 0.0f;
            for (size_t i = 0; i < g.phaseDurations.size(); ++i) cycle += g.phaseDurations[i];
            if (cycle <= 0.0f) return;

            while (offsetSec < 0.0f) offsetSec += cycle;
            while (offsetSec >= cycle) offsetSec -= cycle;

            for (size_t i = 0; i < g.phaseDurations.size(); ++i) {
                float d = g.phaseDurations[i];
                if (offsetSec < d) {
                    g.currentPhaseIndex = (int)i;
                    g.phaseTimer = d - offsetSec;
                    if (g.phaseTimer <= 0.0f) g.phaseTimer = 0.001f;
                    return;
                }
                offsetSec -= d;
            }

            g.currentPhaseIndex = 0;
            g.phaseTimer = g.phaseDurations[0];
        };

        int signalId = 0;
        int groupId = 0;

        for (int hi = 0; hi < numH; ++hi) {
            for (int vi = 0; vi < numV; ++vi) {
                float vr = vx[vi];
                float hr = hy[hi];

                const float rightLaneY = hr + 3.5f;
                const float leftLaneY = hr + 1.5f;
                const float downLaneX = vr + 1.5f;
                const float upLaneX = vr + 3.5f;

                TrafficSignal sLeftToRight;
                sLeftToRight.id = signalId++;
                sLeftToRight.position = Vec2(vr - 1.3f, hr + RW - 0.5f);
                signals.push_back(sLeftToRight);

                TrafficSignal sRightToLeft;
                sRightToLeft.id = signalId++;
                sRightToLeft.position = Vec2(vr + RW + 1.3f, hr + 0.5f);
                signals.push_back(sRightToLeft);

                TrafficSignal sTopToBottom;
                sTopToBottom.id = signalId++;
                sTopToBottom.position = Vec2(vr + 0.5f, hr - 1.3f);
                signals.push_back(sTopToBottom);

                TrafficSignal sBottomToTop;
                sBottomToTop.id = signalId++;
                sBottomToTop.position = Vec2(vr + RW - 0.5f, hr + RW + 1.3f);
                signals.push_back(sBottomToTop);

           

                TrafficSignalGroup hGroup;
                hGroup.groupId = groupId++;
                hGroup.signalIds.push_back(sLeftToRight.id);
                hGroup.signalIds.push_back(sRightToLeft.id);
                hGroup.bindings.push_back(TrafficSignalBinding(sLeftToRight.id, SignalRole::AllVehicle));
                hGroup.bindings.push_back(TrafficSignalBinding(sRightToLeft.id, SignalRole::AllVehicle));
                setupHorizontalGroup(hGroup);

                TrafficSignalGroup vGroup;
                vGroup.groupId = groupId++;
                vGroup.signalIds.push_back(sTopToBottom.id);
                vGroup.signalIds.push_back(sBottomToTop.id);
                vGroup.bindings.push_back(TrafficSignalBinding(sTopToBottom.id, SignalRole::AllVehicle));
                vGroup.bindings.push_back(TrafficSignalBinding(sBottomToTop.id, SignalRole::AllVehicle));
                setupVerticalGroup(vGroup);

                float pairOffset = (float)vi * 4.0f + (float)hi * 1.5f;
                applyOffsetToGroup(hGroup, pairOffset);
                applyOffsetToGroup(vGroup, pairOffset);

                groups.push_back(hGroup);
                groups.push_back(vGroup);
            }
        }

        traffic.init(signals, groups);
    }

    // Course A: vertical roads at {4,17,27,42,53,65}, horizontal roads at {5,19,31}
    void setupTrafficSignals(TrafficSystem& traffic)
    {
        const float vx[] = { 4, 17, 27, 42, 53, 65 };
        const float hy[] = { 5, 19, 31 };
        buildTrafficSignals(traffic, vx, 6, hy, 3, 5.0f);
    }

    // Course B: skip signals at roundabout and non-road intersections
    // (skip logic preserved exactly — only signal positions updated)
    void setupTrafficSignalsB(TrafficSystem& traffic) {
        const float vx[] = { 5, 20, 31, 48, 60, 74 };
        const float hy[] = { 6, 21, 34 };
        const float RW = 5.0f;

        std::vector<TrafficSignal> signals;
        TrafficSignalGroup hGroup, vGroup;
        hGroup.groupId = 0;
        vGroup.groupId = 1;

        int id = 0;
        for (int hi = 0; hi < 3; ++hi) {
            for (int vi = 0; vi < 6; ++vi) {
                // Full skip (no signals at all)
                if (vi == 2 && hi == 0) continue; // roundabout
                if (vi == 1 && hi == 2) continue; // bot-left park area
                if (vi == 5 && hi == 1) continue; // right-mid

                // Per-direction skip: RIGHT=0 LEFT=1 DOWN=2 UP=3
                bool skip[4] = { false, false, false, false };
                if (vi == 1 && hi == 0) skip[2] = true;
                if (vi == 1 && hi == 1) skip[3] = true;
                if (vi == 0 && hi == 2) skip[1] = true;
                if (vi == 2 && hi == 2) skip[0] = true;
                if (vi == 3 && hi == 0) skip[3] = true;
                if (vi == 3 && hi == 1) skip[2] = true;
                if (vi == 4 && hi == 1) skip[1] = true;
                if (vi == 5 && hi == 0) skip[3] = true;
                if (vi == 5 && hi == 2) { skip[2] = true; skip[3] = true; }

                float vr = vx[vi], hr = hy[hi];
                TrafficSignal s;

                if (!skip[0]) { s.id = id++; s.position = Vec2(vr - 1.3f, hr + RW - 0.5f); signals.push_back(s); hGroup.signalIds.push_back(s.id); } else { id++; }
                if (!skip[1]) { s.id = id++; s.position = Vec2(vr + RW + 1.3f, hr + 0.5f); signals.push_back(s); hGroup.signalIds.push_back(s.id); } else { id++; }
                if (!skip[2]) { s.id = id++; s.position = Vec2(vr + 0.5f, hr - 1.3f);       signals.push_back(s); vGroup.signalIds.push_back(s.id); } else { id++; }
                if (!skip[3]) { s.id = id++; s.position = Vec2(vr + RW - 0.5f, hr + RW + 1.3f); signals.push_back(s); vGroup.signalIds.push_back(s.id); } else { id++; }
            }
        }

        hGroup.phaseSequence.push_back(SignalPhase::MainGreen);  hGroup.phaseDurations.push_back(8.0f);
        hGroup.phaseSequence.push_back(SignalPhase::MainYellow); hGroup.phaseDurations.push_back(3.0f);
        hGroup.phaseSequence.push_back(SignalPhase::AllRed);     hGroup.phaseDurations.push_back(2.0f);
        hGroup.currentPhaseIndex = 0; hGroup.phaseTimer = 8.0f;

        vGroup.phaseSequence.push_back(SignalPhase::AllRed);     vGroup.phaseDurations.push_back(10.0f);
        vGroup.phaseSequence.push_back(SignalPhase::MainGreen);  vGroup.phaseDurations.push_back(7.0f);
        vGroup.phaseSequence.push_back(SignalPhase::MainYellow); vGroup.phaseDurations.push_back(3.0f);
        vGroup.currentPhaseIndex = 0; vGroup.phaseTimer = 10.0f;

        std::vector<TrafficSignalGroup> groups;
        groups.push_back(hGroup);
        groups.push_back(vGroup);
        traffic.init(signals, groups);
    }

        void syncCourseCheckpointsToMap(Session& session) {
        const std::vector<Checkpoint>& evalCheckpoints = session.evaluator.getCheckpoints();
        std::vector<Checkpoint>& mapCheckpoints = session.map.getCheckpointsMut();
        const size_t count = std::min(evalCheckpoints.size(), mapCheckpoints.size());
        for (size_t i = 0; i < count; ++i) {
            mapCheckpoints[i].passed = evalCheckpoints[i].passed;
        }
        session.cpDone = session.evaluator.getPassedCheckpointCount();
        session.cpTotal = static_cast<int>(evalCheckpoints.size());
    }

    bool startRoadSession(Session& session) {
        session = Session();
        session.mode = GameMode::RoadDriving;
        session.mapType = MapType::CourseA;

        session.map.generateCourseMap();

        session.engine.init(GameMode::RoadDriving);
        session.engine.setMap(&session.map);
        session.engine.spawnVehicle(session.map.getSpawnPosition(), session.map.getSpawnHeading());

        setupTrafficSignals(session.traffic);

        session.evaluator.setMap(&session.map);
        session.evaluator.setTrafficSystem(&session.traffic);
        session.evaluator.setReplayRecorder(&session.recorder);

        CourseInfo course;
        course.name = "A";
        course.mapFile = "course_a.txt";
        course.checkpoints = session.map.getCheckpoints();
        course.defaultSpeedLimit = 50.0f;
        course.timeLimit = 300.0f;
        session.evaluator.loadCourse(course);
        session.evaluator.reset();

        session.cpTotal = static_cast<int>(session.map.getCheckpoints().size());
        session.finalSignals = session.traffic.getSignals();
        return true;
    }

    bool startParkingSession(Session& session) {
        session = Session();
        session.mode = GameMode::Parking;
        session.mapType = MapType::TParking;

        session.map.generateParkingMap();
        session.engine.init(GameMode::Parking);
        session.engine.setMap(&session.map);
        session.engine.spawnVehicle(session.map.getSpawnPosition(), 0.0f);

        const std::vector<ParkingSlot>& slots = session.map.getParkingSlots();
        for (size_t i = 0; i < slots.size(); ++i) {
            if (slots[i].isTarget) {
                session.targetSlot = slots[i];
                session.targetSlot.isParked = false;   // ߰
                break;
            }
        }

        session.cpTotal = 1;
        return true;
    }

    bool startRoadSessionB(Session& session) {
        session = Session();
        session.mode = GameMode::RoadDriving;
        session.mapType = MapType::CourseB;

        session.map.generateCourseMapB();

        session.engine.init(GameMode::RoadDriving);
        session.engine.setMap(&session.map);
        session.engine.spawnVehicle(session.map.getSpawnPosition(), session.map.getSpawnHeading());

        // FIX: use Course B signal positions instead of Course A
        setupTrafficSignalsB(session.traffic);

        session.evaluator.setMap(&session.map);
        session.evaluator.setTrafficSystem(&session.traffic);
        session.evaluator.setReplayRecorder(&session.recorder);

        CourseInfo course;
        course.name = "B";
        course.mapFile = "course_b.txt";
        course.checkpoints = session.map.getCheckpoints();
        course.defaultSpeedLimit = 50.0f;
        course.timeLimit = 360.0f;
        session.evaluator.loadCourse(course);
        session.evaluator.reset();

        session.cpTotal = static_cast<int>(session.map.getCheckpoints().size());
        session.finalSignals = session.traffic.getSignals();
        return true;
    }

    bool startParallelParkingSession(Session& session) {
        session = Session();
        session.mode = GameMode::Parking;
        session.mapType = MapType::ParallelParking;

        session.map.generateParallelParkingMap();
        session.engine.init(GameMode::Parking);
        session.engine.setMap(&session.map);
        session.engine.spawnVehicle(session.map.getSpawnPosition(), session.map.getSpawnHeading());

        const auto& slots = session.map.getParkingSlots();
        for (size_t i = 0; i < slots.size(); i++) {
            if (slots[i].isTarget) {
                session.targetSlot = slots[i];
                session.targetSlot.isParked = false;   // ?? ߰
                break;
            }
        }

        session.cpTotal = 1;
        return true;
    }

    bool startTParkingSession(Session& session) {
        session = Session();
        session.mode = GameMode::Parking;
        session.mapType = MapType::TParking;

        session.map.generateTParkingMap();
        session.engine.init(GameMode::Parking);
        session.engine.setMap(&session.map);
        session.engine.spawnVehicle(session.map.getSpawnPosition(), session.map.getSpawnHeading());

        const auto& slots = session.map.getParkingSlots();
        for (size_t i = 0; i < slots.size(); i++) {
            if (slots[i].isTarget) {
                session.targetSlot = slots[i];
                session.targetSlot.isParked = false;   // ?? ߰
                break;
            }
        }

        session.cpTotal = 1;
        return true;
    }

    // Restart the current session using the saved mapType
    bool restartCurrentSession(Session& session) {
        MapType mt = session.mapType;
        switch (mt) {
        case MapType::CourseA:          return startRoadSession(session);
        case MapType::CourseB:          return startRoadSessionB(session);
        case MapType::ParallelParking:  return startParallelParkingSession(session);
        case MapType::TParking:         return startTParkingSession(session);
        default:                        return startRoadSession(session);
        }
    }

    void startReplay(Session& session) {
        session.replayPlayer.load(session.recorder.getFrames());
        session.replayPlayer.setSpeed(session.replaySpeed);
        session.replayPlayer.play();
        session.replayPaused = false;
        session.penaltyMarkers = session.replayPlayer.getPenaltyMarkers();
        session.finalSignals = session.traffic.getSignals();
    }

    void applyReplayControls(Session& session,
        bool togglePlay,
        bool prevPenaltyRequested,
        bool nextPenaltyRequested,
        int requestedFrameIndex) {
        if (togglePlay) {
            session.replayPlayer.toggle();
        }
        if (prevPenaltyRequested) session.replayPlayer.jumpToPrevPenalty();
        if (nextPenaltyRequested) session.replayPlayer.jumpToNextPenalty();
        if (requestedFrameIndex >= 0) session.replayPlayer.jumpTo(requestedFrameIndex);
        session.replayPlayer.setSpeed(session.replaySpeed);
        session.replayPaused = !session.replayPlayer.isPlaying();
    }

    void updateRoad(Session& session,
        float dt,
        AppScreen& screen,
        bool& appRunning,
        PrevKeys& prevKeys,
        bool allowDrivingInput) {
        VehicleState& veh = session.engine.getVehicleStateMut();
        InputResult input = processInput(veh, session.warning, session.warningTime, prevKeys, allowDrivingInput);

        if (input.quit) {
            appRunning = false;
            return;
        }
        if (input.restart) {
            restartCurrentSession(session);
            initPrevKeys(prevKeys, captureKeys(true));
            return;
        }

        VehicleControl ctrl = buildControl(input, veh);
        session.engine.tick(dt, ctrl);

        // FIX: update traffic signal phases each frame
        session.traffic.update(dt);

        session.evaluator.evaluate(session.engine.getVehicleState(), ctrl, dt, &session.map.getNpcCars());
        session.penalty = session.evaluator.getTotalPenaltyPoints();
        session.elapsed += dt;
        session.warningTime = std::max(0.0f, session.warningTime - dt);
        session.finalSignals = session.traffic.getSignals();

        const int previousCpDone = session.cpDone;
        syncCourseCheckpointsToMap(session);
        if (session.cpDone > previousCpDone) {
            session.warning = "CHECKPOINT PASSED";
            session.warningTime = 1.4f;
        }

        const VehicleState& vs = session.engine.getVehicleState();
        session.trail.add(static_cast<int>(vs.position.x + 0.5f), static_cast<int>(vs.position.y + 0.5f));

        if (session.engine.getCollisionCount() > 0) {
            session.crashed = true;
            screen = AppScreen::Collision;
            return;
        }

        if (session.evaluator.isDisqualified()) {
            session.disqualified = true;
            session.warning = "DISQUALIFIED!";
            session.warningTime = 2.0f;
            screen = AppScreen::Result;
            return;
        }

        if (session.cpTotal > 0 && session.cpDone >= session.cpTotal) {
            session.warning = "COURSE COMPLETE!";
            session.warningTime = 2.0f;
            screen = AppScreen::Result;
            return;
        }
    }

    void updateParking(Session& session,
        float dt,
        AppScreen& screen,
        bool& appRunning,
        PrevKeys& prevKeys,
        bool allowDrivingInput) {

        VehicleState& veh = session.engine.getVehicleStateMut();
        InputResult input = processInput(veh, session.warning, session.warningTime, prevKeys, allowDrivingInput);

        if (input.quit) {
            appRunning = false;
            return;
        }
        if (input.restart) {
            restartCurrentSession(session);
            initPrevKeys(prevKeys, captureKeys(true));
            return;
        }

        VehicleControl ctrl = buildControl(input, veh);
        session.engine.tick(dt, ctrl);

        const VehicleState& vs = session.engine.getVehicleState();
        session.trail.add(static_cast<int>(vs.position.x + 0.5f), static_cast<int>(vs.position.y + 0.5f));
        session.elapsed += dt;
        session.warningTime = std::max(0.0f, session.warningTime - dt);

        if (session.engine.getCollisionCount() > 0) {
            session.crashed = true;
            screen = AppScreen::Collision;
            return;
        }

        if (session.elapsed >= 0.5f) {
            bool actuallyMoving = (float)fabs(vs.speed) > 0.8f;
            bool reallyStarting = actuallyMoving || (ctrl.accel > 0.2f && vs.engineOn);

            if (reallyStarting && !vs.seatbeltOn) {
                session.disqualified = true;
                session.penalty += session.evaluator.getConfig().noSeatbeltPenalty;
                session.warning = "NO SEATBELT! DISQUALIFIED!";
                session.warningTime = 2.0f;
                screen = AppScreen::Result;
                return;
            }
        }

        if (session.parkingEvaluator.checkComplete(vs, session.targetSlot)) {
            session.parkOk = true;
            session.targetSlot.isParked = true;
            session.warning = "PARKING COMPLETE!";
            session.warningTime = 3.0f;
            session.cpDone = 1;

            if (session.parkSuccessTimer <= 0.0f) {
                session.parkSuccessTimer = 1.2f;   // 1.2  ʷ ۷ο ֱ
            }
        }

        if (session.parkOk) {
            session.parkSuccessTimer -= dt;
            if (session.parkSuccessTimer <= 0.0f) {
                screen = AppScreen::Result;
                return;
            }
        }
    }

    void updateReplay(Session& session,
        float dt,
        AppScreen& screen,
        bool& appRunning,
        PrevKeys& prevKeys,
        bool& togglePlay,
        bool& prevPenaltyRequested,
        bool& nextPenaltyRequested,
        int requestedFrameIndex) {
        KeySnapshot keys = captureKeys(false);
        if ((keys.q && !prevKeys.q) || (keys.esc && !prevKeys.esc)) {
            appRunning = false;
            return;
        }

        if (keys.space && !prevKeys.space) {
            togglePlay = true;
        }
        if (keys.left && !prevKeys.left) {
            prevPenaltyRequested = true;
        }
        if (keys.right && !prevKeys.right) {
            nextPenaltyRequested = true;
        }

        prevKeys.q = keys.q;
        prevKeys.esc = keys.esc;
        prevKeys.space = keys.space;
        prevKeys.left = keys.left;
        prevKeys.right = keys.right;

        applyReplayControls(session, togglePlay, prevPenaltyRequested, nextPenaltyRequested, requestedFrameIndex);

        if (session.replayPlayer.isPlaying()) {
            session.replayPlayer.advance(dt);
        }
        session.replayPaused = !session.replayPlayer.isPlaying();

        if (session.replayPlayer.isFinished()) {
            session.replayPlayer.pause();
            session.replayPaused = true;
        }

        if (session.recorder.getFrames().empty()) {
            screen = AppScreen::Menu;
        }
    }

} // namespace

int main() {
    Renderer renderer;
    if (!renderer.init()) {
        return -1;
    }

    bool appRunning = true;
    AppScreen screen = AppScreen::Menu;
    Session session;
    PrevKeys prevKeys;
    MenuNavState menuNav;

    sf::Clock deltaClock;

    while (appRunning && renderer.isOpen()) {
        if (!renderer.processEvents()) {
            break;
        }

        sf::Time frameTime = deltaClock.restart();
        float dt = frameTime.asSeconds();
        if (dt > 0.1f) dt = 0.1f;
        // Don't simulate NPCs during replay — NPC positions come from the
        // recorded frames and are written into the map before rendering.
        if (screen != AppScreen::Replay) {
            session.map.updateNpcCars(dt,
                session.finalSignals.empty() ? nullptr : &session.finalSignals,
                &session.engine.getVehicleState());
        }

        renderer.beginFrame(sf::seconds(dt));

        const bool allowDrivingInput = (screen == AppScreen::RoadDriving || screen == AppScreen::Parking);

        if (screen == AppScreen::Menu) {
            menuNav = captureMenuNavState(menuNav);

            bool roadARequested = false;
            bool roadBRequested = false;
            bool parallelParkingRequested = false;
            bool tParkingRequested = false;
            bool quitRequested = false;
            renderer.drawMenuUi(menuNav.selectedIndex, roadARequested, roadBRequested,
                parallelParkingRequested, tParkingRequested, quitRequested);

            if (roadARequested) {
                if (startRoadSession(session)) {
                    renderer.setZoom(renderer.recommendZoom(session.map, 0.92f));
                    initPrevKeys(prevKeys, captureKeys(true));
                    screen = AppScreen::RoadDriving;
                }
            }
            if (roadBRequested) {
                if (startRoadSessionB(session)) {
                    renderer.setZoom(renderer.recommendZoom(session.map, 0.92f));
                    initPrevKeys(prevKeys, captureKeys(true));
                    screen = AppScreen::RoadDriving;
                }
            }
            if (parallelParkingRequested) {
                if (startParallelParkingSession(session)) {
                    renderer.setZoom(renderer.recommendZoom(session.map, 0.88f));
                    initPrevKeys(prevKeys, captureKeys(true));
                    screen = AppScreen::Parking;
                }
            }
            if (tParkingRequested) {
                if (startTParkingSession(session)) {
                    renderer.setZoom(renderer.recommendZoom(session.map, 0.88f));
                    initPrevKeys(prevKeys, captureKeys(true));
                    screen = AppScreen::Parking;
                }
            }
            if (quitRequested) {
                appRunning = false;
            }
        }
        else if (screen == AppScreen::RoadDriving) {
            updateRoad(session, dt, screen, appRunning, prevKeys, allowDrivingInput);
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                &session.finalSignals,
                nullptr,
                nullptr,
                &session.map.getCheckpoints());
            bool hudMenuReq = false;
            renderer.drawDrivingUi(session.engine.getVehicleState(),
                session.cpDone,
                session.cpTotal,
                session.penalty,
                session.elapsed,
                session.engine.getCollisionCount(),
                session.warning,
                session.warningTime,
                true,
                hudMenuReq);
            if (hudMenuReq) { menuNav = MenuNavState(); screen = AppScreen::Menu; }
        }
        else if (screen == AppScreen::Parking) {
            updateParking(session, dt, screen, appRunning, prevKeys, allowDrivingInput);
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                nullptr,
                nullptr,
                &session.targetSlot,
                nullptr);
            bool hudMenuReq = false;
            renderer.drawDrivingUi(session.engine.getVehicleState(),
                session.cpDone,
                session.cpTotal,
                session.penalty,
                session.elapsed,
                session.engine.getCollisionCount(),
                session.warning,
                session.warningTime,
                false,
                hudMenuReq);
            if (hudMenuReq) { menuNav = MenuNavState(); screen = AppScreen::Menu; }
        }
        else if (screen == AppScreen::Collision) {
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                session.mode == GameMode::RoadDriving ? &session.finalSignals : nullptr,
                nullptr,
                session.mode == GameMode::Parking ? &session.targetSlot : nullptr,
                session.mode == GameMode::RoadDriving ? &session.map.getCheckpoints() : nullptr);
            bool restartRequested = false;
            bool menuRequested = false;
            bool quitRequested = false;
            renderer.drawCollisionUi(restartRequested, menuRequested, quitRequested);

            if (restartRequested) {
                restartCurrentSession(session);
                float zoomF = (session.mode == GameMode::RoadDriving) ? 0.92f : 0.88f;
                renderer.setZoom(renderer.recommendZoom(session.map, zoomF));
                initPrevKeys(prevKeys, captureKeys(true));
                screen = (session.mode == GameMode::RoadDriving) ? AppScreen::RoadDriving : AppScreen::Parking;
            }
            //정서현 - 충돌 창 → 상세 사유 창 → 메뉴
            if (menuRequested) {
                screen = AppScreen::CollisionDetail;
            }
            if (quitRequested) {
                appRunning = false;
            }
        }
        else if (screen == AppScreen::CollisionDetail) {
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                session.mode == GameMode::RoadDriving ? &session.finalSignals : nullptr,
                nullptr,
                session.mode == GameMode::Parking ? &session.targetSlot : nullptr,
                session.mode == GameMode::RoadDriving ? &session.map.getCheckpoints() : nullptr);

            const std::vector<Penalty>& penaltyLog = session.evaluator.getPenaltyLog();

            std::string collisionReason = "충돌 / 코스 위반";
            if (!penaltyLog.empty()) {
                collisionReason = penaltyLog.back().description;
            }

            bool backRequested = false;
            bool nextToMenuRequested = false;

            renderer.drawCollisionDetailUi(
                "충돌 상세 정보",
                collisionReason,
                penaltyLog,
                backRequested,
                nextToMenuRequested
            );

            if (backRequested) {
                screen = AppScreen::Collision;
            }
            if (nextToMenuRequested) {
                menuNav = MenuNavState();
                screen = AppScreen::Menu;
            }
            }
            
        else if (screen == AppScreen::Result) {
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                session.mode == GameMode::RoadDriving ? &session.finalSignals : nullptr,
                nullptr,
                session.mode == GameMode::Parking ? &session.targetSlot : nullptr,
                session.mode == GameMode::RoadDriving ? &session.map.getCheckpoints() : nullptr);
            //정서현 추가부분
            const std::vector<Penalty>& penaltyLog = session.evaluator.getPenaltyLog();
            ExamResult result = buildExamResult(penaltyLog);

            bool restartRequested = false;
            bool replayRequested = false;
            bool menuRequested = false;
            bool quitRequested = false;
            const bool canReplay = session.mode == GameMode::RoadDriving && !session.recorder.getFrames().empty();
            
            renderer.drawResultUi(
                session.mode == GameMode::RoadDriving ? "도로주행 결과" : "주차 결과",
                session.elapsed,
                session.cpDone,
                session.cpTotal,
                session.penalty,
                session.engine.getCollisionCount(),
                penaltyLog,
                result.disqualified,
                result.passed,
                result.disqualifyReason,
                result.finalScore,
                canReplay,
                restartRequested,
                replayRequested,
                menuRequested,
                quitRequested);

            if (restartRequested) {
                restartCurrentSession(session);
                float zoomF = (session.mode == GameMode::RoadDriving) ? 0.92f : 0.88f;
                renderer.setZoom(renderer.recommendZoom(session.map, zoomF));
                initPrevKeys(prevKeys, captureKeys(true));
                screen = (session.mode == GameMode::RoadDriving) ? AppScreen::RoadDriving : AppScreen::Parking;
            }
            if (replayRequested) {
                startReplay(session);
                initPrevKeys(prevKeys, captureKeys(true));
                screen = AppScreen::Replay;
            }
            //정서현 menu 버튼 눌렀을 때 이동 대상 변경
            if (menuRequested) {
                screen = AppScreen::ResultDetail;
            }
            if (quitRequested) {
                appRunning = false;
            }
        }
        //정서현 ResultDetail 추가
        else if (screen == AppScreen::ResultDetail) {
            renderer.drawWorld(session.map,
                session.engine.getVehicleState(),
                &session.trail,
                session.mode == GameMode::RoadDriving ? &session.finalSignals : nullptr,
                nullptr,
                session.mode == GameMode::Parking ? &session.targetSlot : nullptr,
                session.mode == GameMode::RoadDriving ? &session.map.getCheckpoints() : nullptr);

            const std::vector<Penalty>& penaltyLog = session.evaluator.getPenaltyLog();
            ExamResult result = buildExamResult(penaltyLog);

            bool nextToMenuRequested = false;
            bool replayRequested = false;
            bool backRequested = false;

            const bool canReplay = session.mode == GameMode::RoadDriving && !session.recorder.getFrames().empty();

            renderer.drawResultDetailUi(
                session.mode == GameMode::RoadDriving ? "도로주행 상세 결과" : "주차 상세 결과",
                result.finalScore,
                result.disqualified,
                result.passed,
                result.disqualifyReason,
                penaltyLog,
                nextToMenuRequested,
                replayRequested,
                backRequested,
                canReplay
            );

            if (backRequested) {
                screen = AppScreen::Result;
            }
            if (nextToMenuRequested) {
                menuNav = MenuNavState();
                screen = AppScreen::Menu;
            }
            if (replayRequested) {
                startReplay(session);
                initPrevKeys(prevKeys, captureKeys(true));
                screen = AppScreen::Replay;
            }
            }
        else if (screen == AppScreen::Replay) {
            bool keyboardTogglePlay = false;
            bool keyboardPrevPenalty = false;
            bool keyboardNextPenalty = false;

            updateReplay(session,
                dt,
                screen,
                appRunning,
                prevKeys,
                keyboardTogglePlay,
                keyboardPrevPenalty,
                keyboardNextPenalty,
                -1);

            bool uiTogglePlay = false;
            bool menuRequested = false;
            bool restartRequested = false;
            bool uiPrevPenaltyRequested = false;
            bool uiNextPenaltyRequested = false;
            int requestedFrameIndex = -1;

            const ReplayFrame* frame = session.replayPlayer.getCurrentFrame();
            if (frame) {
                // Swap recorded NPC states into the map so drawNpcCars picks
                // them up via map.getNpcCars().
                session.map.getNpcCarsMut() = frame->npcCars;
                renderer.drawWorld(session.map,
                    frame->vehicle,
                    nullptr,
                    &session.finalSignals,
                    &session.penaltyMarkers,
                    nullptr,
                    &session.map.getCheckpoints());
                renderer.drawReplayUi(frame->timestamp,
                    session.recorder.getFrames().empty() ? 0.0f : session.recorder.getFrames().back().timestamp,
                    session.replaySpeed,
                    session.replayPaused,
                    session.replayPlayer.getCurrentIndex(),
                    session.replayPlayer.getTotalFrames(),
                    uiTogglePlay,
                    menuRequested,
                    restartRequested,
                    uiPrevPenaltyRequested,
                    uiNextPenaltyRequested,
                    requestedFrameIndex);
            }

            if (uiTogglePlay || uiPrevPenaltyRequested || uiNextPenaltyRequested || requestedFrameIndex >= 0) {
                applyReplayControls(session,
                    uiTogglePlay,
                    uiPrevPenaltyRequested,
                    uiNextPenaltyRequested,
                    requestedFrameIndex);
            }
            if (menuRequested) {
                menuNav = MenuNavState();
                screen = AppScreen::Menu;
            }
            if (restartRequested) {
                restartCurrentSession(session);
                float zoomF = (session.mode == GameMode::RoadDriving) ? 0.92f : 0.88f;
                renderer.setZoom(renderer.recommendZoom(session.map, zoomF));
                initPrevKeys(prevKeys, captureKeys(true));
                screen = (session.mode == GameMode::RoadDriving) ? AppScreen::RoadDriving : AppScreen::Parking;
            }
        }

        renderer.endFrame();
    }

    return 0;
}
/*. CollisionDetail, ResultDetail, ExamResult, 확장된 drawResultUi(...) 호출추가*/