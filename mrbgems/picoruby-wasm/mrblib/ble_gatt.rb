module JS
  module BLE
    module GATT
      # Request a BLE device via browser picker.
      # Must be called from a user gesture handler (click, etc.).
      #
      # @param name [String, nil] exact device name filter
      # @param name_prefix [String, nil] device name prefix filter
      # @param services [Array<String>, nil] required service UUIDs
      # @param optional_services [Array<String>, nil] optional service UUIDs
      # @return [JS::BLE::GATT::Device]
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
        uuids.each { |u| js_arr.push(normalize_uuid(u)) }
        js_arr
      end

      def self.request_device(name: nil, name_prefix: nil,
                              services: nil, optional_services: nil)
        navigator = JS.global[:navigator]
        bluetooth = navigator[:bluetooth]
        raise "Web Bluetooth API not supported" unless bluetooth
        options = JS.global.create_object

        if name || name_prefix || services
          filters = JS.global.create_array
          filter = JS.global.create_object
          filter[:name] = name if name
          filter[:namePrefix] = name_prefix if name_prefix
          if services
            filter[:services] = uuids_to_js_array(services)
          end
          filters.push(filter)
          options[:filters] = filters
        else
          options[:acceptAllDevices] = true
        end

        if optional_services
          options[:optionalServices] = uuids_to_js_array(optional_services)
        end

        device = nil
        bluetooth.requestDevice(options).then do |js_device|
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
        # @return [JS::BLE::GATT::Server]
        def connect
          server = nil
          @js_device[:gatt].connect.then do |js_server|
            server = Server.new(js_server)
          end
          server
        end

        # Disconnect from the GATT server.
        def disconnect
          @js_device[:gatt].disconnect
        end

        # Whether the device is currently connected.
        # @return [Boolean]
        def connected?
          @js_device[:gatt][:connected] == true
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
        # @param uuid [String]
        # @return [JS::BLE::GATT::Service]
        def service(uuid)
          svc = nil
          @js_server.getPrimaryService(GATT.normalize_uuid(uuid)).then do |js_service|
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
        # @param uuid [String]
        # @return [JS::BLE::GATT::Characteristic]
        def characteristic(uuid)
          char = nil
          @js_service.getCharacteristic(GATT.normalize_uuid(uuid)).then do |js_char|
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
        # @return [String] binary string
        def read
          data = nil
          @js_char.readValue.then do |js_dataview|
            data = JS::BLE._dataview_to_string(js_dataview)
          end
          data
        end

        # Write data to the characteristic.
        # @param data [String] binary string to write
        # @param without_response [Boolean] use writeValueWithoutResponse
        def write(data, without_response: false)
          js_uint8 = JS::BLE._string_to_uint8array(data)
          if without_response
            @js_char.writeValueWithoutResponse(js_uint8).then {}
          else
            @js_char.writeValueWithResponse(js_uint8).then {}
          end
        end

        # Register a notification callback.
        # The block receives binary String data on each notification.
        # @yield [String] binary notification data
        def on_change(&block)
          callback_id = block.object_id
          JS::Object::CALLBACKS[callback_id] = block
          JS::BLE._set_notify_handler(@js_char, callback_id)
        end

        # Start receiving notifications.
        def start_notify
          @js_char.startNotifications.then {}
        end

        # Stop receiving notifications.
        def stop_notify
          @js_char.stopNotifications.then {}
        end
      end
    end
  end
end
