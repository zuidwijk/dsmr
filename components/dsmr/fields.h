/**
 * Arduino DSMR parser.
 *
 * This software is licensed under the MIT License.
 *
 * Copyright (c) 2015 Matthijs Kooijman <matthijs@stdin.nl>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Field parsing functions
 */

#pragma once

#include "util.h"
#include "parser.h"

#ifndef DSMR_GAS_MBUS_ID
#define DSMR_GAS_MBUS_ID 1
#endif
#ifndef DSMR_WATER_MBUS_ID
#define DSMR_WATER_MBUS_ID 2
#endif
#ifndef DSMR_THERMAL_MBUS_ID
#define DSMR_THERMAL_MBUS_ID 3
#endif
#ifndef DSMR_SUB_MBUS_ID
#define DSMR_SUB_MBUS_ID 4
#endif

namespace dsmr
{

  /**
 * Superclass for data items in a P1 message.
 */
  template <typename T>
  struct ParsedField
  {
    template <typename F>
    void apply(F &f) { f.apply(*static_cast<T *>(this)); }
    // By defaults, fields have no unit
    static const char *unit() { return ""; }
  };

  template <typename T, size_t minlen, size_t maxlen>
  struct StringField : ParsedField<T>
  {
    ParseResult<void> parse(const char *str, const char *end)
    {
      ParseResult<String> res = StringParser::parse_string(minlen, maxlen, str, end);
      if (!res.err)
        static_cast<T *>(this)->val() = res.result;
      return res;
    }
  };

  // A timestamp is essentially a string using YYMMDDhhmmssX format (where
  // X is W or S for wintertime or summertime). Parsing this into a proper
  // (UNIX) timestamp is hard to do generically. Parsing it into a
  // single integer needs > 4 bytes top fit and isn't very useful (you
  // cannot really do any calculation with those values). So we just parse
  // into a string for now.
  template <typename T>
  struct TimestampField : StringField<T, 13, 13>
  {
  };

  // Value that is parsed as a three-decimal float, but stored as an
  // integer (by multiplying by 1000). Supports val() (or implicit cast to
  // float) to get the original value, and int_val() to get the more
  // efficient integer value. The unit() and int_unit() methods on
  // FixedField return the corresponding units for these values.
  struct FixedValue
  {
    operator float() { return val(); }
    float val() { return _value / 1000.0; }
    uint32_t int_val() { return _value; }

    uint32_t _value;
  };

  // Floating point numbers in the message never have more than 3 decimal
  // digits. To prevent inefficient floating point operations, we store
  // them as a fixed-point number: an integer that stores the value in
  // thousands. For example, a value of 1.234 kWh is stored as 1234. This
  // effectively means that the integer value is the value in Wh. To allow
  // automatic printing of these values, both the original unit and the
  // integer unit is passed as a template argument.
  template <typename T, const char *_unit, const char *_int_unit>
  struct FixedField : ParsedField<T>
  {
    ParseResult<void> parse(const char *str, const char *end)
    {
      // Check if the value is a float value, plus its expected unit type.
      ParseResult<uint32_t> res_float = NumParser::parse(3, _unit, str, end);
      if (!res_float.err) {
        static_cast<T *>(this)->val()._value = res_float.result;
        return res_float;
      }
      // If not, then check for an int value, plus its expected unit type.
      // This accomodates for some smart meters that publish int values instead
      // of floats. E.g. most meters would publish "1-0:1.8.0(000441.879*kWh)",
      // but some use "1-0:1.8.0(000441879*Wh)" instead.
      ParseResult<uint32_t> res_int = NumParser::parse(0, _int_unit, str, end);
      if (!res_int.err) {
        static_cast<T *>(this)->val()._value = res_int.result;
        return res_int;
      }
      // If not, then return the initial error result for the float parsing step.
      return res_float;
    }

    static const char *unit() { return _unit; }
    static const char *int_unit() { return _int_unit; }
  };

  struct TimestampedFixedValue : public FixedValue
  {
    String timestamp;
  };

  // Some numerical values are prefixed with a timestamp. This is simply
  // both of them concatenated, e.g. 0-1:24.2.1(150117180000W)(00473.789*m3)
  template <typename T, const char *_unit, const char *_int_unit>
  struct TimestampedFixedField : public FixedField<T, _unit, _int_unit>
  {
    ParseResult<void> parse(const char *str, const char *end)
    {
      // First, parse timestamp
      ParseResult<String> res = StringParser::parse_string(13, 13, str, end);
      if (res.err)
        return res;

      static_cast<T *>(this)->val().timestamp = res.result;

      // Which is immediately followed by the numerical value
      return FixedField<T, _unit, _int_unit>::parse(res.next, end);
    }
  };

  // A integer number is just represented as an integer.
  template <typename T, const char *_unit>
  struct IntField : ParsedField<T>
  {
    ParseResult<void> parse(const char *str, const char *end)
    {
      ParseResult<uint32_t> res = NumParser::parse(0, _unit, str, end);
      if (!res.err)
        static_cast<T *>(this)->val() = res.result;
      return res;
    }

    static const char *unit() { return _unit; }
  };

  // A RawField is not parsed, the entire value (including any
  // parenthesis around it) is returned as a string.
  template <typename T>
  struct RawField : ParsedField<T>
  {
    ParseResult<void> parse(const char *str, const char *end)
    {
      // Just copy the string verbatim value without any parsing
      concat_hack(static_cast<T *>(this)->val(), str, end - str);
      return ParseResult<void>().until(end);
    }
  };

  namespace fields
  {

    struct units
    {
      // These variables are inside a struct, since that allows us to make
      // them constexpr and define their values here, but define the storage
      // in a cpp file. Global const(expr) variables have implicitly
      // internal linkage, meaning each cpp file that includes us will have
      // its own copy of the variable. Since we take the address of these
      // variables (passing it as a template argument), this would cause a
      // compiler warning. By putting these in a struct, this is prevented.
      static constexpr char none[] = "";
      static constexpr char kWh[] = "kWh";
      static constexpr char Wh[] = "Wh";
      static constexpr char kW[] = "kW";
      static constexpr char W[] = "W";
      static constexpr char V[] = "V";
      static constexpr char mV[] = "mV";
      static constexpr char A[] = "A";
      static constexpr char mA[] = "mA";
      static constexpr char m3[] = "m3";
      static constexpr char dm3[] = "dm3";
      static constexpr char GJ[] = "GJ";
      static constexpr char MJ[] = "MJ";
      static constexpr char kvar[] = "kvar";
      static constexpr char kvarh[] = "kvarh";
      static constexpr char Hz[] = "Hz";
    };

    const uint8_t GAS_MBUS_ID = DSMR_GAS_MBUS_ID;
    const uint8_t WATER_MBUS_ID = DSMR_WATER_MBUS_ID;
    const uint8_t THERMAL_MBUS_ID = DSMR_THERMAL_MBUS_ID;
    const uint8_t SUB_MBUS_ID = DSMR_SUB_MBUS_ID;

#define DEFINE_FIELD(fieldname, value_t, obis, field_t, field_args...) \
  struct fieldname : field_t<fieldname, ##field_args>                  \
  {                                                                    \
    value_t fieldname;                                                 \
    bool fieldname##_present = false;                                  \
    static constexpr ObisId id = obis;                                 \
    static constexpr char name[] = #fieldname;                         \
    value_t &val() { return fieldname; }                               \
    bool &present() { return fieldname##_present; }                    \
  }

    /* Meter identification. This is not a normal field, but a
 * specially-formatted first line of the message */
    DEFINE_FIELD(identification, String, ObisId(255, 255, 255, 255, 255, 255), RawField);

    /* Version information for P1 output */
    DEFINE_FIELD(p1_version, String, ObisId(1, 3, 0, 2, 8), StringField, 2, 2);
    DEFINE_FIELD(p1_version_be, String, ObisId(0, 0, 96, 1, 4), StringField, 2, 5);

    /* Date-time stamp of the P1 message */
    DEFINE_FIELD(timestamp, String, ObisId(0, 0, 1, 0, 0), TimestampField);

    /* Equipment identifier equipment_id 96.1.0 */
    // DEFINE_FIELD(equipment_id, String, ObisId(0, 0, 96, 1, 1), StringField, 0, 96);
    DEFINE_FIELD(equipment_id, String, ObisId(0, 0, 96, 1, 0), StringField, 0, 96);

   
    /* Positive active energy (A+) total [kWh] */
    DEFINE_FIELD(energy_delivered, FixedValue, ObisId(1, 0, 1, 8, 0), FixedField, units::kWh, units::Wh);
    /* Positive active energy (A+) in tariff T1 [kWh] */
    DEFINE_FIELD(energy_delivered_tariff1, FixedValue, ObisId(1, 0, 1, 8, 1), FixedField, units::kWh, units::Wh);
    /* Positive active energy (A+) in tariff T2 [kWh] */
    DEFINE_FIELD(energy_delivered_tariff2, FixedValue, ObisId(1, 0, 1, 8, 2), FixedField, units::kWh, units::Wh);
    /* Positive active energy (A+) in tariff T3 [kWh] */
    DEFINE_FIELD(energy_delivered_tariff3, FixedValue, ObisId(1, 0, 1, 8, 3), FixedField, units::kWh, units::Wh);
    /* Positive active energy (A+) in tariff T4 [kWh] */
    DEFINE_FIELD(energy_delivered_tariff4, FixedValue, ObisId(1, 0, 1, 8, 4), FixedField, units::kWh, units::Wh);

    /* Negative active energy (A+) total [kWh] */
    DEFINE_FIELD(energy_returned, FixedValue, ObisId(1, 0, 2, 8, 0), FixedField, units::kWh, units::Wh);
    /* Negative active energy (A+) in tariff T1 [kWh] */
    DEFINE_FIELD(energy_returned_tariff1, FixedValue, ObisId(1, 0, 2, 8, 1), FixedField, units::kWh, units::Wh);
    /* Negative active energy (A+) in tariff T2 [kWh] */
    DEFINE_FIELD(energy_returned_tariff2, FixedValue, ObisId(1, 0, 2, 8, 2), FixedField, units::kWh, units::Wh);
    /* Negative active energy (A+) in tariff T3 [kWh] */
    DEFINE_FIELD(energy_returned_tariff3, FixedValue, ObisId(1, 0, 2, 8, 3), FixedField, units::kWh, units::Wh);
    /* Negative active energy (A+) in tariff T4 [kWh] */
    DEFINE_FIELD(energy_returned_tariff4, FixedValue, ObisId(1, 0, 2, 8, 4), FixedField, units::kWh, units::Wh);

    /* Absolute active energy (A+) total [kWh] */
    DEFINE_FIELD(energy_absolute, FixedValue, ObisId(1, 0, 15, 8, 0), FixedField, units::kWh, units::Wh);
    /* Absolute active energy (A+) in tariff T1 [kWh] */
    DEFINE_FIELD(energy_absolute_tariff1, FixedValue, ObisId(1, 0, 15, 8, 1), FixedField, units::kWh, units::Wh);
    /* Absolute active energy (A+) in tariff T2 [kWh] */
    DEFINE_FIELD(energy_absolute_tariff2, FixedValue, ObisId(1, 0, 15, 8, 2), FixedField, units::kWh, units::Wh);
    /* Absolute active energy (A+) in tariff T3 [kWh] */
    DEFINE_FIELD(energy_absolute_tariff3, FixedValue, ObisId(1, 0, 15, 8, 3), FixedField, units::kWh, units::Wh);
    /* Absolute active energy (A+) in tariff T4 [kWh] */
    DEFINE_FIELD(energy_absolute_tariff4, FixedValue, ObisId(1, 0, 15, 8, 4), FixedField, units::kWh, units::Wh);

    /* Sum active energy without reverse blockade (A+ - A-) total [kWh] */
    DEFINE_FIELD(energy_sumwoblockade, FixedValue, ObisId(1, 0, 16, 8, 0), FixedField, units::kWh, units::Wh);
    /* Sum active energy without reverse blockade (A+ - A-) in tariff T1 [kWh] */
    DEFINE_FIELD(energy_sumwoblockade_tariff1, FixedValue, ObisId(1, 0, 16, 8, 1), FixedField, units::kWh, units::Wh);
    /* Sum active energy without reverse blockade (A+ - A-) in tariff T2 [kWh] */
    DEFINE_FIELD(energy_sumwoblockade_tariff2, FixedValue, ObisId(1, 0, 16, 8, 2), FixedField, units::kWh, units::Wh);
    /* Sum active energy without reverse blockade (A+ - A-) in tariff T3 [kWh] */
    DEFINE_FIELD(energy_sumwoblockade_tariff3, FixedValue, ObisId(1, 0, 16, 8, 3), FixedField, units::kWh, units::Wh);
    /* Sum active energy without reverse blockade (A+ - A-) in tariff T4 [kWh] */
    DEFINE_FIELD(energy_sumwoblockade_tariff4, FixedValue, ObisId(1, 0, 16, 8, 4), FixedField, units::kWh, units::Wh);

    /* Positive reactive energy (Q+) total [kvarh] 3.8.0 */
    // DEFINE_FIELD(energy_positive_reactive, FixedValue, ObisId(1, 0, 3, 8, 0), FixedField, units::kvarh, units::kvarh);
    /* Positive reactive energy (Q+) total in tariff T1 [kvarh] */
    DEFINE_FIELD(energy_positive_reactive_tariff1, FixedValue, ObisId(1, 0, 3, 8, 1), FixedField, units::kvarh, units::kvarh);
    /* Positive reactive energy (Q+) total in tariff T2 [kvarh] */
    DEFINE_FIELD(energy_positive_reactive_tariff2, FixedValue, ObisId(1, 0, 3, 8, 2), FixedField, units::kvarh, units::kvarh);
    /* Positive reactive energy (Q+) total in tariff T3 [kvarh] */
    DEFINE_FIELD(energy_positive_reactive_tariff3, FixedValue, ObisId(1, 0, 3, 8, 3), FixedField, units::kvarh, units::kvarh);
    /* Positive reactive energy (Q+) total in tariff T4 [kvarh] */
    DEFINE_FIELD(energy_positive_reactive_tariff4, FixedValue, ObisId(1, 0, 3, 8, 4), FixedField, units::kvarh, units::kvarh);
    
    /* Negative reactive energy (Q+) total [kvarh] 4.8.0 */
    DEFINE_FIELD(energy_negative_reactive, FixedValue, ObisId(1, 0, 4, 8, 0), FixedField, units::kvarh, units::kvarh);
    /* negative reactive energy (Q+) total in tariff T1 [kvarh] */
    DEFINE_FIELD(energy_negative_reactive_tariff1, FixedValue, ObisId(1, 0, 4, 8, 1), FixedField, units::kvarh, units::kvarh);
    /* Negative reactive energy (Q+) total in tariff T2 [kvarh] */
    DEFINE_FIELD(energy_negative_reactive_tariff2, FixedValue, ObisId(1, 0, 4, 8, 2), FixedField, units::kvarh, units::kvarh);
    /* Negative reactive energy (Q+) total in tariff T3 [kvarh] */
    DEFINE_FIELD(energy_negative_reactive_tariff3, FixedValue, ObisId(1, 0, 4, 8, 3), FixedField, units::kvarh, units::kvarh);
    /* Negative reactive energy (Q+) total in tariff T4 [kvarh] */
    DEFINE_FIELD(energy_negative_reactive_tariff4, FixedValue, ObisId(1, 0, 4, 8, 4), FixedField, units::kvarh, units::kvarh);
    
    /* Tariff indicator electricity. 96.14.0
    * The tariff indicator can also be used to switch tariff dependent loads e.g boilers.
    * This is the responsibility of the P1 user */
    DEFINE_FIELD(electricity_tariff, uint8_t, ObisId(0, 0, 96, 14, 0), IntField, units::none);

    /* current status fo the breaker ON/OFF */
    DEFINE_FIELD(breaker_status, String, ObisId(0, 0, 96, 50, 68), StringField, 2, 3);

    /* Actual electricity power delivered (+P) in 1 Watt resolution 1.7.0 */
    DEFINE_FIELD(power_delivered, FixedValue, ObisId(1, 0, 1, 7, 0), FixedField, units::kW, units::W);
    /* Actual electricity power received (-P) in 1 Watt resolution 2.7.0 */
    DEFINE_FIELD(power_returned, FixedValue, ObisId(1, 0, 2, 7, 0), FixedField, units::kW, units::W);

    /* 5.7.0(@1,Reactive power QI,kvar,pwr_reac_q1,3 [kvarh] */
    DEFINE_FIELD(reactive_power_qi, FixedValue, ObisId(1, 0, 5, 7, 0), FixedField, units::kvar, units::kvar);
    /* 6.7.0(@1,Reactive power QII,kvar,pwr_reac_q2,3 */
    DEFINE_FIELD(reactive_power_qii, FixedValue, ObisId(1, 0, 6, 7, 0), FixedField, units::kvar, units::kvar);
    /* 7.7.0(@1,Reactive power QIII,kvar,pwr_reac_q3,3 */
    DEFINE_FIELD(reactive_power_qiii, FixedValue, ObisId(1, 0, 7, 7, 0), FixedField, units::kvar, units::kvar);
    /* 8.7.0(@1,Reactive power QIV,kvar,pwr_reac_q4,3 */
    DEFINE_FIELD(reactive_power_qiv, FixedValue, ObisId(1, 0, 8, 7, 0), FixedField, units::kvar, units::kvar);

    /* 5.8.0(@1,Reactive energy QI,kvarh,nrg_reac_q1,3 [kvarh] */
    DEFINE_FIELD(reactive_energy_qi, FixedValue, ObisId(1, 0, 5, 8, 0), FixedField, units::kvarh, units::kvarh);
    /* 6.8.0(@1,Reactive energy QII,kvarh,nrg_reac_q2,3 [kvarh] */
    DEFINE_FIELD(reactive_energy_qii, FixedValue, ObisId(1, 0, 6, 8, 0), FixedField, units::kvarh, units::kvarh);
    /* 7.8.0(@1,Reactive energy QIII,kvarh,nrg_reac_q3,3 [kvarh] */
    DEFINE_FIELD(reactive_energy_qiii, FixedValue, ObisId(1, 0, 7, 8, 0), FixedField, units::kvarh, units::kvarh);
    /* 8.8.0(@1,Reactive energy QIV,kvarh,nrg_reac_q4,3 [kvarh] */
    DEFINE_FIELD(reactive_energy_qiv, FixedValue, ObisId(1, 0, 8, 8, 0), FixedField, units::kvarh, units::kvarh);


    /*
 * Extra fields used for Luxembourg
 */
    DEFINE_FIELD(reactive_power_delivered, FixedValue, ObisId(1, 0, 3, 8, 0), FixedField, units::kvarh, units::kvarh);

    DEFINE_FIELD(reactive_power_returned, FixedValue, ObisId(1, 0, 4, 7, 0), FixedField, units::kvar, units::kvar);

    /* 13.7.0	Instantaneous power factor */
    DEFINE_FIELD(instantaneous_power_factor, FixedValue, ObisId(1, 0, 13, 7, 0), FixedField, units::none, units::none);
    /* 33.7.0	Instantaneous power factor L1 */
    DEFINE_FIELD(instantaneous_power_factor_l1, FixedValue, ObisId(1, 0, 33, 7, 0), FixedField, units::none, units::none);
    /* 53.7.0	Instantaneous power factor L2 */
    DEFINE_FIELD(instantaneous_power_factor_l2, FixedValue, ObisId(1, 0, 53, 7, 0), FixedField, units::none, units::none);
    /* 73.7.0	Instantaneous power factor L3 */
    DEFINE_FIELD(instantaneous_power_factor_l3, FixedValue, ObisId(1, 0, 73, 7, 0), FixedField, units::none, units::none);

    /* 14.7.0	Frequency [Hz] */
    DEFINE_FIELD(frequency, FixedValue, ObisId(1, 0, 14, 7, 0), FixedField, units::Hz, units::Hz);

    // /* 15.8.0	Absolute active energy (A+) total [kWh] */
    // DEFINE_FIELD(absolute_active_energy, FixedValue, ObisId(1, 0, 15, 8, 0), FixedField, units::kW, units::W);

    /* The actual threshold Electricity in kW. 17.0.0 - Removed in 4.0.7 / 4.2.2 / 5.0 */
    DEFINE_FIELD(electricity_threshold, FixedValue, ObisId(0, 0, 17, 0, 0), FixedField, units::kW, units::W);

    /* Switch position Electricity (in/out/enabled). Removed in 4.0.7 / 4.2.2 / 5.0 */
    DEFINE_FIELD(electricity_switch_position, uint8_t, ObisId(0, 0, 96, 3, 10), IntField, units::none);

    /* Number of power failures in any phase */
    DEFINE_FIELD(electricity_failures, uint32_t, ObisId(0, 0, 96, 7, 21), IntField, units::none);
    /* Number of long power failures in any phase */
    DEFINE_FIELD(electricity_long_failures, uint32_t, ObisId(0, 0, 96, 7, 9), IntField, units::none);

    /* Power Failure Event Log (long power failures) */
    DEFINE_FIELD(electricity_failure_log, String, ObisId(1, 0, 99, 97, 0), RawField);

    /* Number of voltage sags in phase L1 */
    DEFINE_FIELD(electricity_sags_l1, uint32_t, ObisId(1, 0, 32, 32, 0), IntField, units::none);
    /* Number of voltage sags in phase L2 (polyphase meters only) */
    DEFINE_FIELD(electricity_sags_l2, uint32_t, ObisId(1, 0, 52, 32, 0), IntField, units::none);
    /* Number of voltage sags in phase L3 (polyphase meters only) */
    DEFINE_FIELD(electricity_sags_l3, uint32_t, ObisId(1, 0, 72, 32, 0), IntField, units::none);

    /* Number of voltage swells in phase L1 */
    DEFINE_FIELD(electricity_swells_l1, uint32_t, ObisId(1, 0, 32, 36, 0), IntField, units::none);
    /* Number of voltage swells in phase L2 (polyphase meters only) */
    DEFINE_FIELD(electricity_swells_l2, uint32_t, ObisId(1, 0, 52, 36, 0), IntField, units::none);
    /* Number of voltage swells in phase L3 (polyphase meters only) */
    DEFINE_FIELD(electricity_swells_l3, uint32_t, ObisId(1, 0, 72, 36, 0), IntField, units::none);

    /* Text message codes: numeric 8 digits (Note: Missing from 5.0 spec)
 * */
    DEFINE_FIELD(message_short, String, ObisId(0, 0, 96, 13, 1), StringField, 0, 16);
    /* Text message max 2048 characters (Note: Spec says 1024 in comment and
 * 2048 in format spec, so we stick to 2048). */
    DEFINE_FIELD(message_long, String, ObisId(0, 0, 96, 13, 0), StringField, 0, 2048);

    /* Instantaneous voltage L1 in 0.1V resolution (Note: Spec says V
 * resolution in comment, but 0.1V resolution in format spec. Added in
 * 5.0) */ /* 32.7.0 */
    DEFINE_FIELD(voltage_l1, FixedValue, ObisId(1, 0, 32, 7, 0), FixedField, units::V, units::mV);
    /* Instantaneous voltage L2 in 0.1V resolution (Note: Spec says V
 * resolution in comment, but 0.1V resolution in format spec. Added in
 * 5.0) */ /* 52.7.0 */
    DEFINE_FIELD(voltage_l2, FixedValue, ObisId(1, 0, 52, 7, 0), FixedField, units::V, units::mV);
    /* Instantaneous voltage L3 in 0.1V resolution (Note: Spec says V
 * resolution in comment, but 0.1V resolution in format spec. Added in
 * 5.0) */ /* 72.7.0 */
    DEFINE_FIELD(voltage_l3, FixedValue, ObisId(1, 0, 72, 7, 0), FixedField, units::V, units::mV);

    /* Instantaneous current L1 in A resolution */
    DEFINE_FIELD(current_l1, FixedValue, ObisId(1, 0, 31, 7, 0), FixedField, units::A, units::mA);
    /* Instantaneous current L2 in A resolution */
    DEFINE_FIELD(current_l2, FixedValue, ObisId(1, 0, 51, 7, 0), FixedField, units::A, units::mA);
    /* Instantaneous current L3 in A resolution */
    DEFINE_FIELD(current_l3, FixedValue, ObisId(1, 0, 71, 7, 0), FixedField, units::A, units::mA); 

    /* Instantaneous active power L1 (+P) in W resolution */
    DEFINE_FIELD(power_delivered_l1, FixedValue, ObisId(1, 0, 21, 7, 0), FixedField, units::kW, units::W);
    /* Instantaneous active power L2 (+P) in W resolution */
    DEFINE_FIELD(power_delivered_l2, FixedValue, ObisId(1, 0, 41, 7, 0), FixedField, units::kW, units::W);
    /* Instantaneous active power L3 (+P) in W resolution */
    DEFINE_FIELD(power_delivered_l3, FixedValue, ObisId(1, 0, 61, 7, 0), FixedField, units::kW, units::W);

    /* Instantaneous active power L1 (-P) in W resolution */
    DEFINE_FIELD(power_returned_l1, FixedValue, ObisId(1, 0, 22, 7, 0), FixedField, units::kW, units::W);
    /* Instantaneous active power L2 (-P) in W resolution */
    DEFINE_FIELD(power_returned_l2, FixedValue, ObisId(1, 0, 42, 7, 0), FixedField, units::kW, units::W);
    /* Instantaneous active power L3 (-P) in W resolution */
    DEFINE_FIELD(power_returned_l3, FixedValue, ObisId(1, 0, 62, 7, 0), FixedField, units::kW, units::W);

    /*
 * LUX
 */
    /* Instantaneous reactive power L1 (+Q) in W resolution */
    DEFINE_FIELD(reactive_power_delivered_l1, FixedValue, ObisId(1, 0, 23, 7, 0), FixedField, units::none, units::none);
    /* Instantaneous reactive power L2 (+Q) in W resolution */
    DEFINE_FIELD(reactive_power_delivered_l2, FixedValue, ObisId(1, 0, 43, 7, 0), FixedField, units::none, units::none);
    /* Instantaneous reactive power L3 (+Q) in W resolution */
    DEFINE_FIELD(reactive_power_delivered_l3, FixedValue, ObisId(1, 0, 63, 7, 0), FixedField, units::none, units::none);

    /*
 * LUX
 */
    /* Instantaneous reactive power L1 (-Q) in W resolution */
    DEFINE_FIELD(reactive_power_returned_l1, FixedValue, ObisId(1, 0, 24, 7, 0), FixedField, units::none, units::none);
    /* Instantaneous reactive power L2 (-Q) in W resolution */
    DEFINE_FIELD(reactive_power_returned_l2, FixedValue, ObisId(1, 0, 44, 7, 0), FixedField, units::none, units::none);
    /* Instantaneous reactive power L3 (-Q) in W resolution */
    DEFINE_FIELD(reactive_power_returned_l3, FixedValue, ObisId(1, 0, 64, 7, 0), FixedField, units::none, units::none);



    /* Device-Type */
    DEFINE_FIELD(gas_device_type, uint16_t, ObisId(0, GAS_MBUS_ID, 24, 1, 0), IntField, units::none);

    /* Equipment identifier (Gas) */
    DEFINE_FIELD(gas_equipment_id, String, ObisId(0, GAS_MBUS_ID, 96, 1, 0), StringField, 0, 96);
    /* Equipment identifier (Gas) BE */
    DEFINE_FIELD(gas_equipment_id_be, String, ObisId(0, GAS_MBUS_ID, 96, 1, 1), StringField, 0, 96);

    /* Valve position Gas (on/off/released) (Note: Removed in 4.0.7 / 4.2.2 / 5.0). */
    DEFINE_FIELD(gas_valve_position, uint8_t, ObisId(0, GAS_MBUS_ID, 24, 4, 0), IntField, units::none);

    /* Last 5-minute value (temperature converted), gas delivered to client
 * in m3, including decimal values and capture time (Note: 4.x spec has
 * "hourly value") */
    DEFINE_FIELD(gas_delivered, TimestampedFixedValue, ObisId(0, GAS_MBUS_ID, 24, 2, 1), TimestampedFixedField, units::m3,
                 units::dm3);
    /* _BE */
    DEFINE_FIELD(gas_delivered_be, TimestampedFixedValue, ObisId(0, GAS_MBUS_ID, 24, 2, 3), TimestampedFixedField,
                 units::m3, units::dm3);
    DEFINE_FIELD(gas_delivered_text, String, ObisId(0, GAS_MBUS_ID, 24, 3, 0), RawField);

    /* Device-Type */
    DEFINE_FIELD(thermal_device_type, uint16_t, ObisId(0, THERMAL_MBUS_ID, 24, 1, 0), IntField, units::none);

    /* Equipment identifier (Thermal: heat or cold) */
    DEFINE_FIELD(thermal_equipment_id, String, ObisId(0, THERMAL_MBUS_ID, 96, 1, 0), StringField, 0, 96);

    /* Valve position (on/off/released) (Note: Removed in 4.0.7 / 4.2.2 / 5.0). */
    DEFINE_FIELD(thermal_valve_position, uint8_t, ObisId(0, THERMAL_MBUS_ID, 24, 4, 0), IntField, units::none);

    /* Last 5-minute Meter reading Heat or Cold in 0,01 GJ and capture time
 * (Note: 4.x spec has "hourly meter reading") */
    DEFINE_FIELD(thermal_delivered, TimestampedFixedValue, ObisId(0, THERMAL_MBUS_ID, 24, 2, 1), TimestampedFixedField,
                 units::GJ, units::MJ);

    /* Device-Type */
    DEFINE_FIELD(water_device_type, uint16_t, ObisId(0, WATER_MBUS_ID, 24, 1, 0), IntField, units::none);

    /* Equipment identifier (Thermal: heat or cold) */
    DEFINE_FIELD(water_equipment_id, String, ObisId(0, WATER_MBUS_ID, 96, 1, 0), StringField, 0, 96);

    /* Valve position (on/off/released) (Note: Removed in 4.0.7 / 4.2.2 / 5.0). */
    DEFINE_FIELD(water_valve_position, uint8_t, ObisId(0, WATER_MBUS_ID, 24, 4, 0), IntField, units::none);

    /* Last 5-minute Meter reading in 0,001 m3 and capture time
 * (Note: 4.x spec has "hourly meter reading") */
    DEFINE_FIELD(water_delivered, TimestampedFixedValue, ObisId(0, WATER_MBUS_ID, 24, 2, 1), TimestampedFixedField,
                 units::m3, units::dm3);

    /* Device-Type */
    DEFINE_FIELD(sub_device_type, uint16_t, ObisId(0, SUB_MBUS_ID, 24, 1, 0), IntField, units::none);

    /* Equipment identifier (Thermal: heat or cold) */
    DEFINE_FIELD(sub_equipment_id, String, ObisId(0, SUB_MBUS_ID, 96, 1, 0), StringField, 0, 96);

    /* Valve position (on/off/released) (Note: Removed in 4.0.7 / 4.2.2 / 5.0). */
    DEFINE_FIELD(sub_valve_position, uint8_t, ObisId(0, SUB_MBUS_ID, 24, 4, 0), IntField, units::none);

    /* Last 5-minute Meter reading Heat or Cold and capture time (e.g. sub
 * E meter) (Note: 4.x spec has "hourly meter reading") */
    DEFINE_FIELD(sub_delivered, TimestampedFixedValue, ObisId(0, SUB_MBUS_ID, 24, 2, 1), TimestampedFixedField,
                 units::m3, units::dm3);


  } // namespace fields

} // namespace dsmr
