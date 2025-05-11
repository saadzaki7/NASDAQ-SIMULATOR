#include "order_book.h"
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(cpp_order_book, m) {
    m.doc() = "C++ Order Book implementation for high-frequency trading";
    
    py::class_<hft::OrderBook>(m, "OrderBook")
        .def(py::init<>())
        .def("process_message", &hft::OrderBook::process_message, "Process a single message")
        .def("get_order_book_snapshot", &hft::OrderBook::get_order_book_snapshot, "Get order book snapshot")
        .def("get_best_prices", &hft::OrderBook::get_best_prices, "Get best bid and ask prices")
        .def("get_volumes", &hft::OrderBook::get_volumes, "Get total bid and ask volumes")
        .def("get_imbalance", &hft::OrderBook::get_imbalance, "Get order book imbalance");
}
