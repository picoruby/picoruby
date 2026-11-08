# Thread (IEEE 802.15.4) networking, via OpenThread.
#
# The module is not a class because the stack is not one: OpenThread is
# built here with OPENTHREAD_CONFIG_MULTIPLE_INSTANCE_ENABLE off, so
# there is exactly one instance per boot. That also makes this a drop-in
# peer of CYW43, which Network aliases the same way.
#
# Naming: this is deliberately not called Thread. In picoruby the
# concurrency primitive is Task -- Task::Queue is documented as the
# rename of CRuby's Thread::Queue -- and reusing the word for a radio
# protocol would undo that. Network::Thread exists as the role-name
# alias, in the same place Network::WiFi does.
module OpenThread
  # RuntimeError, not StandardError: picoruby-network declares
  # Network::ConnectTimeout as a RuntimeError and both CYW43 and
  # ESP32 raise one, so a rescue written against the shared name has to
  # catch this too.
  class ConnectTimeout < RuntimeError; end

  ROLES = [:disabled, :detached, :child, :router, :leader]

  # 802.15.4 channels in the 2.4 GHz band.
  CHANNEL_MIN = 11
  CHANNEL_MAX = 26

  def self.init
    __init
  end

  # Install the Active Dataset.
  #
  # network_key is the only field a node genuinely needs -- everything
  # else can be discovered -- but supplying pan_id and channel skips an
  # MLE Announce scan across every channel, which is the difference
  # between attaching in a second and attaching in a minute.
  #
  # tlvs: an Active Dataset as raw TLV *bytes*, for a dataset produced
  # elsewhere. Note that `ot-ctl dataset active -x` prints hex text --
  # decode it first, because OpenThread treats unknown TLV types as
  # valid, so hex passed through verbatim is accepted and configures the
  # node with nonsense.
  def self.configure(network_key: nil, pan_id: nil, channel: nil, tlvs: nil)
    if tlvs
      validate_tlvs!(tlvs)
      __set_dataset_tlvs(tlvs)
      return true
    end
    unless network_key.is_a?(String) && network_key.bytesize == 16
      raise ArgumentError, "network_key must be a 16-byte String"
    end
    if channel && (channel < CHANNEL_MIN || CHANNEL_MAX < channel)
      raise ArgumentError, "channel must be #{CHANNEL_MIN}..#{CHANNEL_MAX}"
    end
    if pan_id && (pan_id < 0 || 0xFFFF < pan_id)
      raise ArgumentError, "pan_id must be 0..0xFFFF"
    end
    __set_dataset(network_key, pan_id || -1, channel || -1)
  end

  # OpenThread accepts unknown TLV types as valid, so a blob that is not
  # a dataset at all -- hex text pasted from `ot-ctl dataset active -x`
  # is the likely case -- can tile the buffer by accident and configure
  # the node with nonsense while reporting success. Walk it first: a
  # real dataset consists of (type, length, value) runs that end exactly
  # on the last byte.
  def self.validate_tlvs!(tlvs)
    unless tlvs.is_a?(String) && !tlvs.empty?
      raise ArgumentError, "tlvs must be a non-empty String of raw bytes"
    end
    i = 0
    size = tlvs.bytesize
    while i < size
      unless i + 2 <= size
        raise ArgumentError, "malformed Active Dataset TLVs (truncated header)"
      end
      len = tlvs.getbyte(i + 1) || 0
      i += 2 + len
    end
    unless i == size
      raise ArgumentError, "malformed Active Dataset TLVs (length overruns by #{i - size})"
    end
    true
  end

  # Order matters and is not ours to choose: otThreadSetEnabled answers
  # OT_ERROR_INVALID_STATE unless the interface is already up.
  def self.start
    __ip6_enable(true)
    __thread_enable(true)
    true
  end

  def self.stop
    __thread_enable(false)
    __ip6_enable(false)
    true
  end

  def self.role
    ROLES[__role] || :disabled
  end

  def self.attached?
    r = __role
    r != 0 && r != 1     # neither disabled nor detached
  end

  # Block until this node has joined, or give up.
  #
  # Waits on the role-change queue rather than polling a predicate: the
  # stack tells us when it moves, so a node that attaches in a second
  # does not sit through ten sleeps first, and one that takes thirty does
  # not wake three hundred times to ask.
  #
  # Returns true, or raises ConnectTimeout. Note this is not the shape of
  # CYW43.connect_timeout, which returns a bool -- CYW43 raises from
  # connect_with_dhcp instead. Raising is the better fit here because a
  # Thread node has no separate "got an address" step to hang it on.
  def self.wait_attached(timeout_ms: 30_000)
    return true if attached?
    queue = (@role_queue ||= begin
      q = Task::Queue.new
      self.__role_queue = q
      q
    end)
    deadline = timeout_ms
    while deadline > 0
      started = Machine.board_millis
      # A nil pop is the timeout; anything else is a role change, and
      # attached? is re-read rather than trusted from the payload so a
      # transition we did not wake for is still seen.
      popped = queue.pop(false, timeout_ms: deadline)
      __role_event_popped unless popped.nil?
      return true if attached?
      deadline -= (Machine.board_millis - started)
    end
    raise ConnectTimeout, "not attached after #{timeout_ms} ms (role: #{role})"
  end

  # A UDP socket over the Thread mesh.
  #
  # The method set mirrors CRuby's UDPSocket so the API is familiar --
  # not so that picoruby-socket programs port unchanged, which naming the
  # class differently defeats anyway. It is deliberately NOT installed as
  # a top-level UDPSocket: mruby/c's
  # define_class returns an existing class rather than refusing, so a
  # later `require "socket"` would graft lwIP's C methods onto this one
  # and reinterpret its data pointer.
  class UDP
    # recvfrom(maxlen) -> [data, [af, port, host, host]]
    #
    # Blocks the Task, not the CPU: sleep_ms parks it and lets the
    # scheduler run, which is also what pumps OpenThread. A closed
    # socket raises IOError rather than looping forever on a nil that
    # can never become a datagram.
    def recvfrom(maxlen = 512, flags = 0)
      while true
        result = recvfrom_nonblock(maxlen)
        return result if result
        sleep_ms 10
      end
      # Not reached: the loop leaves only by returning a datagram or by
      # recvfrom_nonblock raising. Present so the declared return type
      # holds without widening it to nil.
      raise IOError, "unreachable"
    end

    def read(maxlen = 512)
      data, _addr = recvfrom(maxlen)
      data
    end
  end
end

