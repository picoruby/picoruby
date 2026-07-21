require 'ble'

class DemoCentral < BLE
  TARGET_NAME = "PicoRuby"

  def initialize
    super(:central)
    @reported = false
  end

  def heartbeat_callback
    puts "[central] state=#{state}"
  end

  def advertising_report_callback(adv_report)
    return unless adv_report.name_include?(TARGET_NAME)
    puts "[central] found `#{TARGET_NAME}` rssi=#{adv_report.rssi}, connecting..."
    connect(adv_report)
  end

  def packet_callback(event_packet)
    super
    return if @reported || state != :TC_IDLE
    @reported = true
    if services.empty?
      puts "[central] no services discovered"
      return
    end
    services.each do |svc|
      puts "[central] service uuid32=0x#{svc[:uuid32]&.to_s(16)}"
      svc[:characteristics].each do |chr|
        puts "[central]   characteristic uuid32=0x#{chr[:uuid32]&.to_s(16)} value=#{chr[:value].inspect}"
      end
    end
  end
end

# GATT_EVENT_NOTIFICATION is not wired up in the current mrblib central state
# machine (see ble_central.rb), so this demo only proves scan -> connect ->
# discover -> read-once. It stops after a single discovery pass.
central = DemoCentral.new
central.debug = true
central.scan(timeout_ms: 30_000, stop_state: :TC_IDLE)
