#include "agg.hpp"
#include "utils.hpp"

namespace vaex {

template <class T = uint64_t, class BinIndexType = default_index_type, bool FlipEndian = false>
class BinnerOrdinal : public Binner {
  public:
    // format of bins is [nan, null, bin0, bin1, ..., binN-1, out of range]
    using index_type = BinIndexType;
    BinnerOrdinal(int threads, std::string expression, int64_t ordinal_count, int64_t min_value = 0)
        : Binner(threads, expression), ordinal_count(ordinal_count), min_value(min_value), data_ptr(threads), data_size(threads), data_mask_ptr(threads), data_mask_size(threads) {}
    BinnerOrdinal *copy() { return new BinnerOrdinal(*this); }
    virtual ~BinnerOrdinal() {}
    virtual void to_bins(int thread, uint64_t offset, index_type *output, uint64_t length, uint64_t stride) {
        T *data_ptr = this->data_ptr[thread];
        auto data_mask_ptr = this->data_mask_ptr[thread];
        if (data_mask_ptr) {
            for (uint64_t i = offset; i < offset + length; i++) {
                int64_t value = data_ptr[i] - min_value;
                if (FlipEndian) {
                    value = _to_native<>(value);
                }
                index_type index = 0;
                // this followes numpy, 1 is masked
                bool masked = data_mask_ptr[i] == 1;
                if (value != value) {               // nan goes to index 0
                } else if (masked || (value < 0)) { // negative values are interpreted as null
                    index = 1;
                } else {
                    index = index > ordinal_count ? ordinal_count + 2 : value + 2;
                }
                output[i - offset] += index * stride;
            }
        } else {
            for (uint64_t i = offset; i < offset + length; i++) {
                int64_t value = data_ptr[i] - min_value;
                if (FlipEndian) {
                    value = _to_native<>(value);
                }
                index_type index = 0;
                if (value != value) {   // nan goes to index 0
                } else if (value < 0) { // negative values are interpreted as null
                    index = 1;
                } else {
                    index = index > ordinal_count ? ordinal_count + 2 : value + 2;
                }
                output[i - offset] += index * stride;
            }
        }
    }
    virtual uint64_t data_length(int thread) const { return data_size[thread]; };
    virtual uint64_t shape() const { return ordinal_count + 3; }
    void set_data(int thread, py::buffer ar) {
        py::buffer_info info = ar.request();
        if (info.ndim != 1) {
            throw std::runtime_error("Expected a 1d array");
        }
        if (info.itemsize != sizeof(T)) {
            throw std::runtime_error("Itemsize of data and binner are not equal");
        }
        this->data_ptr[thread] = (T *)info.ptr;
        this->data_size[thread] = info.shape[0];
    }
    void clear_data_mask(int thread) {
        this->data_mask_ptr[thread] = nullptr;
        this->data_mask_size[thread] = 0;
    }
    void set_data_mask(int thread, py::buffer ar) {
        py::buffer_info info = ar.request();
        if (info.ndim != 1) {
            throw std::runtime_error("Expected a 1d array");
        }
        this->data_mask_ptr[thread] = (uint8_t *)info.ptr;
        this->data_mask_size[thread] = info.shape[0];
    }
    int64_t ordinal_count;
    int64_t min_value;
    std::vector<T *> data_ptr;
    std::vector<uint64_t> data_size;
    std::vector<uint8_t *> data_mask_ptr;
    std::vector<uint64_t> data_mask_size;
};

template <class T, bool FlipEndian = false>
void add_binner_ordinal_(py::module &m, py::class_<Binner> &base, std::string postfix) {
    typedef BinnerOrdinal<T, default_index_type, FlipEndian> Type;
    std::string class_name = "BinnerOrdinal_" + postfix;
    py::class_<Type>(m, class_name.c_str(), base)
        .def(py::init<int, std::string, int64_t, int64_t>())
        .def("set_data", &Type::set_data)
        .def("clear_data_mask", &Type::clear_data_mask)
        .def("set_data_mask", &Type::set_data_mask)
        .def("copy", &Type::copy)
        .def("__len__", [](const Type &binner) { return binner.ordinal_count + 3; })
        .def_property_readonly("expression", [](const Type &binner) { return binner.expression; })
        .def_property_readonly("ordinal_count", [](const Type &binner) { return binner.ordinal_count; })
        .def_property_readonly("min_value", [](const Type &binner) { return binner.min_value; })
        .def(py::pickle(
            [](const Type &binner) { // __getstate__
                /* Return a tuple that fully encodes the state of the object */
                return py::make_tuple(binner.threads, binner.expression, binner.ordinal_count, binner.min_value);
            },
            [](py::tuple t) { // __setstate__
                if (t.size() != 4)
                    throw std::runtime_error("Invalid state!");
                Type binner(t[0].cast<int>(), t[1].cast<std::string>(), t[2].cast<T>(), t[3].cast<T>());
                return binner;
            }));
    ;
}

template <class T>
void add_binner_ordinal(py::module &m, py::class_<Binner> &base) {
    std::string postfix(type_name<T>::value);
    add_binner_ordinal_<T, false>(m, base, postfix);
    add_binner_ordinal_<T, true>(m, base, postfix + "_non_native");
}

void add_binner_ordinal(py::module &m, py::class_<Binner> &binner) {
#define create(type) add_binner_ordinal<type>(m, binner);
#include "create_alltypes.hpp"
}

} // namespace vaex