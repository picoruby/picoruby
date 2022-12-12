class ConsumerKey
  KEYCODE = {
    # These keys exist in Keyboard::KEYCODE
    # :KC_KB_MUTE          => 0x07F,
    # :KC_KB_VOLUME_UP     => 0x080,
    # :KC_KB_VOLUME_DOWN   => 0x081,
    :KC_SYSTEM_POWER       => 0x081,
    :KC_SYSTEM_SLEEP       => 0x082,
    :KC_SYSTEM_WAKE        => 0x083,
    :KC_MEDIA_FAST_FORWARD => 0x0b3,
    :KC_MEDIA_REWIND       => 0x0b4,
    :KC_MEDIA_NEXT_TRACK   => 0x0B5,
    :KC_MEDIA_PREV_TRACK   => 0x0B6,
    :KC_MEDIA_STOP         => 0x0B7,
    :KC_MEDIA_EJECT        => 0x0B8,
    :KC_MEDIA_PLAY_PAUSE   => 0x0CD,
    :KC_MEDIA_SELECT       => 0x183,
    :KC_AUDIO_MUTE         => 0x0E2,
    :KC_AUDIO_VOL_UP       => 0x0E9,
    :KC_AUDIO_VOL_DOWN     => 0x0EA,
    :KC_BRIGHTNESS_UP      => 0x06F,
    :KC_BRIGHTNESS_DOWN    => 0x070,
    :KC_MAIL               => 0x18B,
    :KC_CALCULATOR         => 0x192,
    :KC_MY_COMPUTER        => 0x194,
    :KC_WWW_SEARCH         => 0x221,
    :KC_WWW_HOME           => 0x223,
    :KC_WWW_BACK           => 0x224,
    :KC_WWW_FORWARD        => 0x225,
    :KC_WWW_STOP           => 0x226,
    :KC_WWW_REFRESH        => 0x227,
    :KC_WWW_FAVORITES      => 0x22A
  }

  ALIAS = {
    :KC_PWR  => 0x081,
    :KC_SLEP => 0x082,
    :KC_WAKE => 0x083,
    :KC_MFFD => 0x0b3,
    :KC_MRWD => 0x0b4,
    :KC_MNXT => 0x0B5,
    :KC_MPRV => 0x0B6,
    :KC_MSTP => 0x0B7,
    :KC_EJCT => 0x0B8,
    :KC_MPLY => 0x0CD,
    :KC_MSEL => 0x183,
    :KC_MUTE => 0x0E2,
    :KC_VOLU => 0x0E9,
    :KC_VOLD => 0x0EA,
    :KC_BRIU => 0x06F,
    :KC_BRID => 0x070,
    # KC_MAIL has no alias
    :KC_CALC => 0x192,
    :KC_MYCM => 0x194,
    :KC_WSCH => 0x221,
    :KC_WHOM => 0x223,
    :KC_WBAK => 0x224,
    :KC_WFWD => 0x225,
    :KC_WSTP => 0x226,
    :KC_WREF => 0x227,
    :KC_WFAV => 0x22A
  }

  VIACODE = {
    0xA5 => :KC_SYSTEM_POWER      ,
    0xA6 => :KC_SYSTEM_SLEEP      ,
    0xA7 => :KC_SYSTEM_WAKE       ,
    0xBB => :KC_MEDIA_FAST_FORWARD,
    0xBC => :KC_MEDIA_REWIND      ,
    0xAB => :KC_MEDIA_NEXT_TRACK  ,
    0xAC => :KC_MEDIA_PREV_TRACK  ,
    0xAD => :KC_MEDIA_STOP        ,
    0xB0 => :KC_MEDIA_EJECT       ,
    0xAE => :KC_MEDIA_PLAY_PAUSE  ,
    0xAF => :KC_MEDIA_SELECT      ,
    0xA8 => :KC_AUDIO_MUTE        ,
    0xA9 => :KC_AUDIO_VOL_UP      ,
    0xAA => :KC_AUDIO_VOL_DOWN    ,
    0xBD => :KC_BRIGHTNESS_UP     ,
    0xBE => :KC_BRIGHTNESS_DOWN   ,
    0xB1 => :KC_MAIL              ,
    0xB2 => :KC_CALCULATOR        ,
    0xB3 => :KC_MY_COMPUTER       ,
    0xB4 => :KC_WWW_SEARCH        ,
    0xB5 => :KC_WWW_HOME          ,
    0xB6 => :KC_WWW_BACK          ,
    0xB7 => :KC_WWW_FORWARD       ,
    0xB8 => :KC_WWW_STOP          ,
    0xB9 => :KC_WWW_REFRESH       ,
    0xBA => :KC_WWW_FAVORITES
  }

  MAP_OFFSET = 0x300

  def self.keycode(sym)
    ConsumerKey::KEYCODE[sym] || ConsumerKey::ALIAS[sym]
  end

  def self.keycode_from_mapcode(mapcode)
    mapcode - MAP_OFFSET
  end

  def self.viacode_from_mapcode(mapcode)
    VIACODE.key(KEYCODE.key(mapcode - MAP_OFFSET) || :KC_NO)
  end

end
