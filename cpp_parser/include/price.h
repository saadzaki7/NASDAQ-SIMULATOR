#pragma once

#include <cstdint>
#include <string>

namespace itch {

// Price with 4 decimal places
class Price4 {
private:
    uint32_t value;

public:
    Price4() : value(0) {}
    explicit Price4(uint32_t val) : value(val) {}
    
    uint32_t raw() const { return value; }
    
    // Convert to decimal string representation
    std::string to_string() const {
        std::string result = std::to_string(value);
        
        // Ensure we have at least 5 digits (one before decimal point and 4 after)
        while (result.length() < 5) {
            result = "0" + result;
        }
        
        // Insert decimal point
        size_t decimal_pos = result.length() - 4;
        result.insert(decimal_pos, ".");
        
        return result;
    }
};

// Price with 8 decimal places
class Price8 {
private:
    uint64_t value;

public:
    Price8() : value(0) {}
    explicit Price8(uint64_t val) : value(val) {}
    
    uint64_t raw() const { return value; }
    
    // Convert to decimal string representation
    std::string to_string() const {
        std::string result = std::to_string(value);
        
        // Ensure we have at least 9 digits (one before decimal point and 8 after)
        while (result.length() < 9) {
            result = "0" + result;
        }
        
        // Insert decimal point
        size_t decimal_pos = result.length() - 8;
        result.insert(decimal_pos, ".");
        
        return result;
    }
};

} // namespace itch
