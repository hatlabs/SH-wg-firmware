#include "n2k_nmea0183_transform.h"

#include <N2kMessages.h>
#include <NMEA0183Messages.h>

const double kPi = 3.14159265358979323846;

const double rad_to_deg = 180.0 / kPi;

void N2KTo0183Transform::set_input(tN2kMsg new_value, uint8_t input_channel) {
  switch (new_value.PGN) {
    case 127250:
      handle_heading(new_value);
      break;
    case 127258:
      handle_variation(new_value);
      break;
    case 128259:
      handle_boat_speed(new_value);
      break;
    case 128267:
      handle_depth(new_value);
      break;
    case 129025:
      handle_position(new_value);
      break;
    case 129026:
      handle_cogsog(new_value);
      break;
    case 129029:
      handle_gnss(new_value);
      break;
    case 130306:
      handle_wind(new_value);
      break;
    default:
      break;
  }
}

void N2KTo0183Transform::handle_heading(const tN2kMsg& msg) {
  unsigned char SID;
  tN2kHeadingReference ref;
  double deviation = 0;
  double variation;
  tNMEA0183Msg nmea0183_msg;

  if (ParseN2kHeading(msg, SID, heading_, deviation, variation, ref)) {
    if (ref == N2khr_magnetic) {
      if (!N2kIsNA(variation)) {
        variation_ = variation;  // Update Variation
      }
      if (!N2kIsNA(heading_) && !N2kIsNA(variation_)) {
        heading_ -= variation_;
      }
    }
    last_heading_elapsed_ = 0;
    if (NMEA0183SetHDG(nmea0183_msg, heading_, deviation, variation_)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::handle_variation(const tN2kMsg& msg) {
  unsigned char SID;
  tN2kMagneticVariation source;

  ParseN2kMagneticVariation(msg, SID, source, days_since_1970_, variation_);
}

void N2KTo0183Transform::handle_boat_speed(const tN2kMsg& msg) {
  unsigned char SID;
  double water_referenced;
  double ground_referenced;
  tN2kSpeedWaterReferenceType swrt;

  if (ParseN2kBoatSpeed(msg, SID, water_referenced, ground_referenced, swrt)) {
    tNMEA0183Msg nmea0183_msg;
    double MagneticHeading =
        (!N2kIsNA(heading_) && !N2kIsNA(variation_) ? heading_ + variation_
                                                    : NMEA0183DoubleNA);
    if (NMEA0183SetVHW(nmea0183_msg, heading_, MagneticHeading,
                       water_referenced)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::handle_depth(const tN2kMsg& msg) {
  unsigned char SID;
  double depth_below_transducer_;
  double offset;
  double range;

  if (ParseN2kWaterDepth(msg, SID, depth_below_transducer_, offset, range)) {
    tNMEA0183Msg nmea0183_msg;
    if (NMEA0183SetDPT(nmea0183_msg, depth_below_transducer_, offset)) {
      emit_0183_string(nmea0183_msg);
    }
    if (NMEA0183SetDBx(nmea0183_msg, depth_below_transducer_, offset)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::handle_position(const tN2kMsg& msg) {
  if (ParseN2kPGN129025(msg, latitude_, longitude_)) {
    last_position_elapsed_ = 0;
  }
}

void N2KTo0183Transform::handle_cogsog(const tN2kMsg& msg) {
  unsigned char SID;
  tN2kHeadingReference heading_reference;
  tNMEA0183Msg nmea0183_msg;

  if (ParseN2kCOGSOGRapid(msg, SID, heading_reference, cog_, sog_)) {
    last_cogsog_elapsed_ = 0;
    double mcog = (!N2kIsNA(cog_) && !N2kIsNA(variation_) ? cog_ - variation_
                                                          : NMEA0183DoubleNA);
    if (heading_reference == N2khr_magnetic) {
      mcog = cog_;
      if (!N2kIsNA(variation_)) cog_ -= variation_;
    }
    if (NMEA0183SetVTG(nmea0183_msg, cog_, mcog, sog_)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::handle_gnss(const tN2kMsg& msg) {
  unsigned char SID;
  tN2kGNSStype gnss_type;
  tN2kGNSSmethod gnss_method;
  unsigned char num_satellites;
  double hdop;
  double pdop;
  double geoidal_separation;
  unsigned char num_reference_stations;
  tN2kGNSStype reference_station_type;
  uint16_t reference_station_id;
  double age_of_correction;

  if (ParseN2kGNSS(msg, SID, days_since_1970_, seconds_since_midnight_,
                   latitude_, longitude_, altitude_, gnss_type, gnss_method,
                   num_satellites, hdop, pdop, geoidal_separation,
                   num_reference_stations, reference_station_type,
                   reference_station_id, age_of_correction)) {
    last_position_elapsed_ = 0;
  }
}

void N2KTo0183Transform::handle_wind(const tN2kMsg& msg) {
  unsigned char SID;
  tN2kWindReference wind_reference;
  tNMEA0183WindReference nmea0183_reference = NMEA0183Wind_True;

  if (ParseN2kWindSpeed(msg, SID, wind_speed_, wind_angle_, wind_reference)) {
    tNMEA0183Msg nmea0183_msg;
    last_wind_elapsed_ = 0;
    if (wind_reference == N2kWind_Apparent)
      nmea0183_reference = NMEA0183Wind_Apparent;

    if (NMEA0183SetMWV(nmea0183_msg, wind_angle_ * rad_to_deg,
                       nmea0183_reference, wind_speed_)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::invalidate_old_data() {
  if (last_heading_elapsed_ > 2000) {
    heading_ = NMEA0183DoubleNA;
  }
  if (last_cogsog_elapsed_ > 2000) {
    cog_ = NMEA0183DoubleNA;
    sog_ = NMEA0183DoubleNA;
  }
  if (last_position_elapsed_ > 4000) {
    latitude_ = NMEA0183DoubleNA;
    longitude_ = NMEA0183DoubleNA;
  }
  if (last_wind_elapsed_ > 2000) {
    wind_speed_ = N2kDoubleNA;
    wind_angle_ = N2kDoubleNA;
  }
}

void N2KTo0183Transform::send_rmc() {
  if (!N2kIsNA(latitude_)) {
    tNMEA0183Msg nmea0183_msg;
    if (NMEA0183SetRMC(nmea0183_msg, seconds_since_midnight_, latitude_,
                       longitude_, cog_, sog_, days_since_1970_, variation_)) {
      emit_0183_string(nmea0183_msg);
    }
  }
}

void N2KTo0183Transform::emit_0183_string(const tNMEA0183Msg& msg) {
  char buf[kMaxNMEA0183MessageSize_];
  if (!msg.GetMessage(buf, kMaxNMEA0183MessageSize_)) {
    debugW("Could not get NMEA 0183 message string");
    return;
  }
  emit(String(buf));
}
