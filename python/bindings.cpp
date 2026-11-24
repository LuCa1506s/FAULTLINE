#include <faultline/simulation.hpp>
#include <pybind11/native_enum.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/stl/filesystem.h>

namespace py = pybind11;
using namespace faultline::sdk;

PYBIND11_MODULE(_native, module) {
    py::register_exception<SnapshotError>(module, "SnapshotError", PyExc_RuntimeError);

    py::native_enum<WorldProfile>(module, "WorldProfile", "enum.Enum")
        .value("STANDARD", WorldProfile::standard)
        .value("COMPACT", WorldProfile::compact)
        .finalize();
    py::class_<Config>(module, "Config")
        .def(py::init<WorldProfile>(), py::arg("profile") = WorldProfile::standard)
        .def_readonly("profile", &Config::profile);
    py::native_enum<Action>(module, "Action", "enum.Enum")
        .value("IDLE", Action::idle)
        .value("EAST", Action::east)
        .value("WEST", Action::west)
        .value("SOUTH", Action::south)
        .value("NORTH", Action::north)
        .finalize();
    py::native_enum<Status>(module, "Status", "enum.Enum")
        .value("RUNNING", Status::running)
        .value("INCAPACITATED", Status::incapacitated)
        .value("TICK_LIMIT", Status::tick_limit)
        .finalize();
    py::native_enum<EventKind>(module, "EventKind", "enum.Enum")
        .value("OPERATION_CREATED", EventKind::operation_created)
        .value("OPERATION_STARTED", EventKind::operation_started)
        .value("OPERATION_RESOLVED", EventKind::operation_resolved)
        .value("OPERATION_FAILED", EventKind::operation_failed)
        .value("OPERATION_CANCELLED", EventKind::operation_cancelled)
        .value("TERRITORY_CHANGED", EventKind::territory_changed)
        .finalize();

    py::class_<Point>(module, "Point")
        .def_readonly("x", &Point::x)
        .def_readonly("z", &Point::z);
    py::class_<Observation>(module, "Observation")
        .def_readonly("tick", &Observation::tick)
        .def_readonly("position", &Observation::position)
        .def_readonly("health", &Observation::health)
        .def_readonly("status", &Observation::status);
    py::class_<RoadEdge>(module, "RoadEdge")
        .def_readonly("first", &RoadEdge::first)
        .def_readonly("second", &RoadEdge::second);
    py::class_<RoadMap>(module, "RoadMap")
        .def_readonly("nodes", &RoadMap::nodes)
        .def_readonly("edges", &RoadMap::edges);
    py::class_<Event>(module, "Event")
        .def_readonly("tick", &Event::tick)
        .def_readonly("kind", &Event::kind)
        .def_readonly("subject", &Event::subject);
    py::class_<StepResult>(module, "StepResult")
        .def_readonly("observation", &StepResult::observation)
        .def_readonly("ticks", &StepResult::ticks)
        .def_readonly("events", &StepResult::events)
        .def_readonly("events_truncated", &StepResult::events_truncated);

    py::class_<Simulation>(module, "Simulation")
        .def(py::init<std::uint64_t, Config>(), py::arg("seed") = 0, py::arg("config") = Config{})
        .def("reset", &Simulation::reset, py::arg("seed"), py::arg("config") = Config{})
        .def("observe", &Simulation::observe)
        .def("road_map", &Simulation::road_map)
        .def("step", &Simulation::step, py::arg("action"), py::arg("ticks") = 1)
        .def("advance", &Simulation::advance, py::arg("ticks"))
        .def("state_hash", &Simulation::state_hash)
        .def("snapshot", [](const Simulation& sim) {
            const auto data = sim.snapshot();
            return py::bytes(reinterpret_cast<const char*>(data.data()), data.size());
        })
        .def("restore", [](Simulation& sim, py::bytes data) {
            char* address{};
            Py_ssize_t size{};
            if (PyBytes_AsStringAndSize(data.ptr(), &address, &size) != 0) throw py::error_already_set{};
            sim.restore({reinterpret_cast<const std::uint8_t*>(address), static_cast<std::size_t>(size)});
        }, py::arg("data").noconvert())
        .def("save", &Simulation::save, py::arg("path"))
        .def("load", &Simulation::load, py::arg("path"))
        .def("close", &Simulation::close)
        .def("__enter__", [](Simulation& sim) -> Simulation& { return sim; }, py::return_value_policy::reference_internal)
        .def("__exit__", [](Simulation& sim, const py::object&, const py::object&, const py::object&) { sim.close(); });
}
