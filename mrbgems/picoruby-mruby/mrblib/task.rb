class Task
  class Stat
    def [](key)
      @data[key]
    end

    def to_s
      str = "tick: #{@data[:tick]}, wakeup_tick: #{@data[:wakeup_tick]}\n"
      [:dormant, :ready, :waiting, :suspended].each do |key|
        str += "#{key}: #{@data[key].size} tasks\n"
        @data[key].each do |task|
          str += "  c_id: #{task[:c_id]}, c_ptr: #{task[:c_ptr].to_s(16)}, c_status: #{task[:c_status]}\n"
          str += "    ptr: #{task[:ptr].to_s(16)}, name: #{task[:name]}, priority: #{task[:priority]}\n"
          str += "    status: #{task[:status]}, reason: #{task[:reason]}\n"
          str += "    timeslice: #{task[:timeslice]},  wakeup_tick: #{task[:wakeup_tick]}\n"
        end
      end
      str
    end
  end
end
