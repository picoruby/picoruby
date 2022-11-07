class Sandbox
  TIMEOUT = 10_000 # 10 sec
  def wait
    n = 0
    # state 0: TASKSTATE_DORMANT == finished
    while self.state != 0 do
      sleep_ms 50
      n += 50
      if TIMEOUT < n
        puts "Error: Timeout (sandbox.state: #{self.state})"
      end
    end
    if e = self.error
      print "=> #{e.message} (#{e.class})"
      return false
    else
      print "=> #{self.result.inspect}"
      return true
    end
    self.suspend
  end
end
