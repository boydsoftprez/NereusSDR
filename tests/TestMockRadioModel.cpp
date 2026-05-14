// tests/TestMockRadioModel.cpp  (NereusSDR)
// NereusSDR-original test helper — no Thetis upstream port.
//
// Provides the moc-generated vtable for TestMockRadioModel (which inherits
// QObject). Qt's AutoMoc requires a dedicated .cpp to anchor the moc output
// when the class is defined in a header shared across multiple test targets.
// Each test that links TestMockRadioModel passes this .cpp as an extra source
// via nereus_add_test(name TestMockRadioModel.cpp).
//
// Phase 3 note: this file was introduced when TestMockRadioModel was refactored
// to inherit QObject (Task 3.1), eliminating the reinterpret_cast<QObject*>
// workaround from Task 1.1.

#include "TestMockRadioModel.h"
