module JS
  module BLE
    module GATT
<<<<<<< HEAD
      # Convert UUID string to the format Web Bluetooth expects.
      # "0xffe0" -> Integer 0xffe0
      # "heart_rate" or "00001234-..." -> pass through as String
      def self.normalize_uuid(uuid)
        if uuid.start_with?("0x") || uuid.start_with?("0X")
          uuid.to_i(16)
        else
          uuid
        end
      end

      # Build a JS array from Ruby array of UUID strings.
      def self.uuids_to_js_array(uuids)
        js_arr = JS.global.create_array
        uuids.each { |u| js_arr.push(normalize_uuid(u)) } # steep:ignore
        js_arr
      end

      # Request a BLE device via browser picker.
      # Must be called from a user gesture handler (click, etc.).
      def self.request_device(name: nil, name_prefix: nil,
                              services: nil, optional_services: nil)
        # @type var navigator: JS::Object
        navigator = JS.global[:navigator] # steep:ignore
        bluetooth = navigator[:bluetooth]
        raise "Web Bluetooth API not supported" unless bluetooth
=======
      # Request a BLE device via browser picker.
      # Must be called from a user gesture handler (click, etc.).
      #
      # @param name [String, nil] exact device name filter
      # @param name_prefix [String, nil] device name prefix filter
      # @param services [Array<String>, nil] required service UUIDs
      # @param optional_services [Array<String>, nil] optional service UUIDs
      # @return [JS::BLE::GATT::Device]
      def self.request_device(name: nil, name_prefix: nil,
                              services: nil, optional_services: nil)
        navigator = JS.global[:navigator]
        raise "Web Bluetooth not available" unless navigator
        bluetooth = navigator[:bluetooth]
        raise "Web Bluetooth API not supported in this browser" unless bluetooth
>>>>>>> f7cfe1d5 (WIP)
        options = JS.global.create_object

        if name || name_prefix || services
          filters = JS.global.create_array
          filter = JS.global.create_object
          filter[:name] = name if name
          filter[:namePrefix] = name_prefix if name_prefix
          if services
<<<<<<< HEAD
            filter[:services] = uuids_to_js_array(services)
=======
            filter[:services] = JS::Bridge.to_js(services)
>>>>>>> f7cfe1d5 (WIP)
          end
          filters.push(filter)
          options[:filters] = filters
        else
          options[:acceptAllDevices] = true
        end

        if optional_services
<<<<<<< HEAD
          options[:optionalServices] = uuids_to_js_array(optional_services)
        end

        device = nil
        bluetooth.requestDevice(options).then do |js_device| # steep:ignore
=======
          options[:optionalServices] = JS::Bridge.to_js(optional_services)
        end

        device = nil
        bluetooth.requestDevice(options).then do |js_device|
>>>>>>> f7cfe1d5 (WIP)
          device = Device.new(js_device)
        end
        device
      end

      # Wraps a BluetoothDevice JS object.
      class Device
        attr_reader :js_device, :name

        def initialize(js_device)
          @js_device = js_device
          name_obj = js_device[:name]
          @name = name_obj ? name_obj.to_s : nil
        end

        # Connect to the GATT server on this device.
<<<<<<< HEAD
        def connect
          # @type var gatt: JS::Object
          gatt = @js_device[:gatt] # steep:ignore
          server = nil
          gatt.connect.then do |js_server| # steep:ignore
=======
        # @return [JS::BLE::GATT::Server]
        def connect
          server = nil
          @js_device[:gatt].connect.then do |js_server|
>>>>>>> f7cfe1d5 (WIP)
            server = Server.new(js_server)
          end
          server
        end

        # Disconnect from the GATT server.
        def disconnect
<<<<<<< HEAD
          # @type var gatt: JS::Object
          gatt = @js_device[:gatt] # steep:ignore
          gatt.disconnect # steep:ignore
=======
          @js_device[:gatt].disconnect
>>>>>>> f7cfe1d5 (WIP)
        end

        # Whether the device is currently connected.
        # @return [Boolean]
        def connected?
<<<<<<< HEAD
          # @type var gatt: JS::Object
          gatt = @js_device[:gatt] # steep:ignore
          gatt[:connected] == true
=======
          @js_device[:gatt][:connected] == true
>>>>>>> f7cfe1d5 (WIP)
        end

        # Register a callback for disconnection events.
        def on_disconnected(&block)
          @js_device.addEventListener("gattserverdisconnected") do |_event|
            block.call
          end
        end
      end

      # Wraps a BluetoothRemoteGATTServer JS object.
      class Server
        attr_reader :js_server

        def initialize(js_server)
          @js_server = js_server
        end

        # Get a primary service by UUID string.
<<<<<<< HEAD
        def service(uuid)
          svc = nil
          @js_server.getPrimaryService(GATT.normalize_uuid(uuid)).then do |js_service| # steep:ignore
=======
        # @param uuid [String]
        # @return [JS::BLE::GATT::Service]
        def service(uuid)
          svc = nil
          @js_server.getPrimaryService(uuid).then do |js_service|
>>>>>>> f7cfe1d5 (WIP)
            svc = Service.new(js_service)
          end
          svc
        end
      end

      # Wraps a BluetoothRemoteGATTService JS object.
      class Service
        attr_reader :js_service

        def initialize(js_service)
          @js_service = js_service
        end

        # Get a characteristic by UUID string.
<<<<<<< HEAD
        def characteristic(uuid)
          char = nil
          @js_service.getCharacteristic(GATT.normalize_uuid(uuid)).then do |js_char| # steep:ignore
=======
        # @param uuid [String]
        # @return [JS::BLE::GATT::Characteristic]
        def characteristic(uuid)
          char = nil
          @js_service.getCharacteristic(uuid).then do |js_char|
>>>>>>> f7cfe1d5 (WIP)
            char = Characteristic.new(js_char)
          end
          char
        end
      end

      # Wraps a BluetoothRemoteGATTCharacteristic JS object.
      class Characteristic
        attr_reader :js_char

        def initialize(js_char)
          @js_char = js_char
        end

        # Read the characteristic value.
<<<<<<< HEAD
        def read
          data = nil
          @js_char.readValue.then do |js_dataview| # steep:ignore
=======
        # @return [String] binary string
        def read
          data = nil
          @js_char.readValue.then do |js_dataview|
>>>>>>> f7cfe1d5 (WIP)
            data = JS::BLE._dataview_to_string(js_dataview)
          end
          data
        end

        # Write data to the characteristic.
<<<<<<< HEAD
        def write(data, without_response: false)
          js_uint8 = JS::BLE._string_to_uint8array(data)
          if without_response
            @js_char.writeValueWithoutResponse(js_uint8).then {} # steep:ignore
          else
            @js_char.writeValueWithResponse(js_uint8).then {} # steep:ignore
=======
        # @param data [String] binary string to write
        # @param without_response [Boolean] use writeValueWithoutResponse
        def write(data, without_response: false)
          js_uint8 = JS::BLE._string_to_uint8array(data)
          if without_response
            @js_char.writeValueWithoutResponse(js_uint8).then {}
          else
            @js_char.writeValueWithResponse(js_uint8).then {}
>>>>>>> f7cfe1d5 (WIP)
          end
        end

        # Register a notification callback.
        # The block receives binary String data on each notification.
<<<<<<< HEAD
=======
        # @yield [String] binary notification data
>>>>>>> f7cfe1d5 (WIP)
        def on_change(&block)
          callback_id = block.object_id
          JS::Object::CALLBACKS[callback_id] = block
          JS::BLE._set_notify_handler(@js_char, callback_id)
        end

        # Start receiving notifications.
        def start_notify
<<<<<<< HEAD
          @js_char.startNotifications.then {} # steep:ignore
=======
          @js_char.startNotifications.then {}
>>>>>>> f7cfe1d5 (WIP)
        end

        # Stop receiving notifications.
        def stop_notify
<<<<<<< HEAD
          @js_char.stopNotifications.then {} # steep:ignore
=======
          @js_char.stopNotifications.then {}
>>>>>>> f7cfe1d5 (WIP)
        end
      end
    end
  end
end
