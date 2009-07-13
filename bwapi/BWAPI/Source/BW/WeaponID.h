#pragma once
#include <Util/Types.h>

namespace BW
{
  /**
   * Enumeration of the weapon types. From DatEdit x1.5
   */
  namespace WeaponID
  {
    enum Enum : u8
    {
      Gauss_Rifle                               = 0x00,
      Gauss_Rifle_Jim_Raynor_Marine             = 0x01,
      C_10_Concussion_Rifle                     = 0x02,
      C_10_Concussion_Rifle_Sarah_Kerrigan      = 0x03,
      Fragmentation_Grenade                     = 0x04,
      Fragmentation_Grenade_Jim_Raynor_Vulture  = 0x05,
      Spider_Mines                              = 0x06,
      Twin_Autocannons                          = 0x07,
      Hellfire_Missile_Pack                     = 0x08,
      Twin_Autocannons_Alan_Schezar             = 0x09,
      Hellfire_Missile_Pack_Alan_Schezar        = 0x0A,
      Arclite_Cannon                            = 0x0B,
      Arclite_Cannon_Edmund_Duke                = 0x0C,
      Fusion_Cutter                             = 0x0D,
      Fusion_Cutter_Harvest                     = 0x0E,
      Gemini_Missiles                           = 0x0F,
      Burst_Lasers                              = 0x10,
      Gemini_Missiles_Tom_Kazansky              = 0x11,
      Burst_Lasers_Tom_Kazansky                 = 0x12,
      ATS_Laser_Battery                         = 0x13,
      ATA_Laser_Battery                         = 0x14,
      ATS_Laser_Battery_Norad_II_Mengsk_DuGalle = 0x15,
      ATA_Laser_Battery_Norad_II_Mengsk_DuGalle = 0x16,
      ATS_Laser_Battery_Hyperion                = 0x17,
      ATA_Laser_Battery_Hyperion                = 0x18,
      Flame_Thrower                             = 0x19,
      Flame_Thrower_Gui_Montag                  = 0x1A,
      Arclite_Shock_Cannon                      = 0x1B,
      Arclite_Shock_Cannon_Edmund_Duke          = 0x1C,
      Longbolt_Missiles                         = 0x1D,
      Yamato_Gun                                = 0x1E,
      Nuclear_Missile                           = 0x1F,
      Lockdown                                  = 0x20,
      EMP_Shockwave                             = 0x21,
      Irradiate                                 = 0x22,
      Claws                                     = 0x23,
      Claws_Devouring_One                       = 0x24,
      Claws_Infested_Kerrigan                   = 0x25,
      Needle_Spines                             = 0x26,
      Needle_Spines_Hunter_Killer               = 0x27,
      Kaiser_Blades                             = 0x28,
      Kaiser_Blades_Torrasque                   = 0x29,
      Toxic_Spores_Broodling                    = 0x2A,
      Spines                                    = 0x2B,
      Spines_Harvest                            = 0x2C,
      Acid_Spray_Unused                         = 0x2D,
      Acid_Spore                                = 0x2E,
      Acid_Spore_Kukulza_Guardian               = 0x2F,
      Glave_Wurm                                = 0x30,
      Glave_Wurm_Kukulza_Mutalisk               = 0x31,
      Venom_Unused_Defiler                      = 0x32,
      Venom_Unused_Defiler_Hero                 = 0x33,
      Seeker_Spores                             = 0x34,
      Subterranean_Tentacle                     = 0x35,
      Suicide_Infested_Terran                   = 0x36,
      Suicide_Scourge                           = 0x37,
      Parasite                                  = 0x38,
      Spawn_Broodlings                          = 0x39,
      Ensnare                                   = 0x3A,
      Dark_Swarm                                = 0x3B,
      Plague                                    = 0x3C,
      Consume                                   = 0x3D,
      Particle_Beam                             = 0x3E,
      Particle_Beam_Harvest                     = 0x3F,
      Psi_Blades                                = 0x40,
      Psi_Blades_Fenix_Zealot                   = 0x41,
      Phase_Disruptor                           = 0x42,
      Phase_Disruptor_Fenix_Dragoon             = 0x43,
      Psi_Assault_Normal_Unused                 = 0x44,
      Psi_Assault_Tassadar_Aldaris              = 0x45,
      Psionic_Shockwave                         = 0x46,
      Psionic_Shockwave_Tassadar_Zeratul_Archon = 0x47,
      Unknown72                                 = 0x48,
      Dual_Photon_Blasters                      = 0x49,
      Anti_matter_Missiles                      = 0x4A,
      Dual_Photon_Blasters_Mojo                 = 0x4B,
      Anti_matter_Missiles_Mojo                 = 0x4C,
      Phase_Disruptor_Cannon                    = 0x4D,
      Phase_Disruptor_Cannon_Danimoth           = 0x4E,
      Pulse_Cannon                              = 0x4F,
      STS_Photon_Cannon                         = 0x50,
      STA_Photon_Cannon                         = 0x51,
      Scarab                                    = 0x52,
      Stasis_Field                              = 0x53,
      Psi_Storm                                 = 0x54,
      Warp_Blades_Zeratul                       = 0x55,
      Warp_Blades_Dark_Templar_Hero             = 0x56,
      Missiles_Unused                           = 0x57,
      Laser_Battery1_Unused                     = 0x58,
      Tormentor_Missiles_Unused                 = 0x59,
      Bombs_Unused                              = 0x5A,
      Raider_Gun_Unused                         = 0x5B,
      Laser_Battery2_Unused                     = 0x5C,
      Laser_Battery3_Unused                     = 0x5D,
      Dual_Photon_Blasters_Unused               = 0x5E,
      Flechette_Grenade_Unused                  = 0x5F,
      Twin_Autocannons_Floor_Trap               = 0x60,
      Hellfire_Missile_Pack_Wall_Trap           = 0x61,
      Flame_Thrower_Wall_Trap                   = 0x62,
      Hellfire_Missile_Pack_Floor_Trap          = 0x63,
      Neutron_Flare                             = 0x64,
      Disruption_Web                            = 0x65,
      Restoration                               = 0x66,
      Halo_Rockets                              = 0x67,
      Corrosive_Acid                            = 0x68,
      Mind_Control                              = 0x69,
      Feedback                                  = 0x6A,
      Optical_Flare                             = 0x6B,
      Maelstrom                                 = 0x6C,
      Subterranean_Spines                       = 0x6D,
      Gauss_Rifle0_Unused                       = 0x6E,
      Warp_Blades                               = 0x6F,
      C_10_Concussion_Rifle_Samir_Duran         = 0x70,
      C_10_Concussion_Rifle_Infested_Duran      = 0x71,
      Dual_Photon_Blasters_Artanis              = 0x72,
      Anti_matter_Missiles_Artanis              = 0x73,
      C_10_Concussion_Rifle_Alexei_Stukov       = 0x74,
      Gauss_Rifle1_Unused                       = 0x75,
      Gauss_Rifle2_Unused                       = 0x76,
      Gauss_Rifle3_Unused                       = 0x77,
      Gauss_Rifle4_Unused                       = 0x78,
      Gauss_Rifle5_Unused                       = 0x79,
      Gauss_Rifle6_Unused                       = 0x7A,
      Gauss_Rifle7_Unused                       = 0x7B,
      Gauss_Rifle8_Unused                       = 0x7C,
      Gauss_Rifle9_Unused                       = 0x7D,
      Gauss_Rifle10_Unused                      = 0x7E,
      Gauss_Rifle11_Unused                      = 0x7F,
      Gauss_Rifle12_Unused                      = 0x80,
      Gauss_Rifle13_Unused                      = 0x81,
      None                                      = 0x82
    };
  };
};