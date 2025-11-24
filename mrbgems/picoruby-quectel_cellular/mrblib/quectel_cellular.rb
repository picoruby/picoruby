require 'uart'

class QuectelCellular

  DEFAULT_LOG_SIZE = 10

  def initialize(uart:, log_size: DEFAULT_LOG_SIZE)
    @uart = uart
    @log = []
    @log_size = log_size
    # These make unstable, I don't know why
#    @uart.clear_tx_buffer
#    @uart.clear_rx_buffer
    @uart.line_ending = "\r"
    @user_agent = "picoruby"
    @apn = "SORACOM.IO"
    @username = "sora"
    @password = "sora"
  end

  attr_accessor :apn, :username, :password, :user_agent
  attr_reader :log

  # PERST pin is used to reset the module.
  # Example:
  # client.perst = GPIO.new(21, GPIO::OUT)
  # client.reset
  def perst=(gpio)
    @perst = gpio
    gpio.write 1
    gpio
  end

  def reset
    raise "perst is not set" unless @perst
    @perst.write 0
    sleep_ms 300
    @perst.write 1
  end

  def configure_and_activate_context
    false # should be implemented in subclass
  end

  def uart_read
    sleep_ms 100
    lines = ""
    while line = @uart.gets
      lines += "< #{line}"
    end
    lines
  end

  def command(cmd, expected_response, error_response = nil, timeout = 5)
    puts cmd
    @uart.puts cmd
    start = Time.new.to_i
    limit = start + timeout
    response = ""
    while Time.new.to_i < limit
      fragment = uart_read
      response << fragment unless fragment.empty?
      if expected_response && response.include?(expected_response)
        puts "response: #{response}"
        puts "OK"
        return true
      elsif error_response && response.include?(error_response)
        return false
      end
      sleep_ms 50  # Prevent busy loop
    end
    response << "\nTimeout!"
    return false
  ensure
    @log << { cmd: cmd, res: response }
    @log.shift if @log.size > @log_size
  end

  def command!(cmd, expected_response, error_response = nil, timeout = 5)
    unless command(cmd, expected_response, error_response, timeout)
      response = (log = @log.last) ? log[:res] : ""
      raise "Command failed: #{cmd}\nResponse: #{response}"
    end
  end

  def check_sim_status
    command!('ATE0', 'OK') # echo off
    command!('AT+CPIN?', '+CPIN: READY', nil, 20)
    command('AT+CIMI', 'OK')
    command!('AT+CGREG?', 'OK', nil, 90)
  end

  # src: local file path eg. /home/cacert.pem
  # dst: remote file path. eg. UFS:cacert.pem
  def fupl(src, dst, chunk_size: 100)
    raise "File not found: #{src}" unless File.exist?(src)
    size = File::Stat.new(src).size
    command!("AT+QFUPL=\"#{dst}\",#{size},80", '>', nil, 10)
    sleep_ms 1000
    File.open(src, "r") do |f| # no "rb" mode in FatFS
      while chunk = f.read(chunk_size)
        @uart.write chunk
        sleep_ms 100
      end
    end
    sleep_ms 1000
    storage, filename = dst.split(':')
    unless filename
      raise "Invalid dst format: #{dst}"
    end
    files = flst(storage)
    unless files.any? { |f| f.include?(filename) }
      raise "Upload verification failed: #{dst} not found"
    end
    dst
  end

  def flst(storage = nil)
    cmd = 'AT+QFLST'
    cmd << "=\"#{storage}:\"" if storage
    return [] unless command(cmd, 'OK')
    (list = @log.last) ? list[:res].split("\n") : []
  end

  # dst: remote file path. eg. UFS:cacert.pem
  def fdel(dst)
    command!("AT+QFDEL=\"#{dst}\"", 'OK', 'ERROR')
    dst
  end

  class UDPClient < QuectelCellular
    def configure_and_activate_context
      command!('ATE0', 'OK') # echo off
      command!("AT+QICSGP=1,1,\"#{@apn}\",\"#{@username}\",\"#{@password}\",0", 'OK', 'ERROR', 10)
      command('AT+QIACT=1', 'OK', 'ERROR', 15)
      command!('AT+QIACT?', '+QIACT: 1,1,1,', nil, 10)
    end

    def send(host, port, data)
      command!("AT+QIOPEN=1,0,\"UDP\",\"#{host}\",#{port},0,0", 'OK', nil, 150)
      command!("AT+QISEND=0,#{data.length}", '>')
      command!(data, 'SEND OK')
    end
  end

  class SoracomBeamUDP < UDPClient
    def send(data)
      super("beam.soracom.io", 23080, data)
    end
  end

  class HTTPSClient < QuectelCellular
    def initialize(cacert:, uart:, log_size: DEFAULT_LOG_SIZE)
      super(uart: uart, log_size: log_size)
      @cacert = cacert # eg. "ufs:cacert.pem" (should be uploaded by fupl method)
    end

    def configure_and_activate_context
      [ 'ATE0', # echo off
        'AT+CCLK?', # check clock
        'AT+QHTTPCFG="contextid",1', # set context id
        'AT+QIACT?', # check context
        "AT+QICSGP=1,1,\"#{@apn}\",\"#{@username}\",\"#{@password}\",1",
        'AT+QIACT=1', # activate context
        'AT+QHTTPCFG="sslctxid",1', # set ssl context id
        'AT+QSSLCFG="sslversion",1,3', # TLS 1.2
        'AT+QSSLCFG="ciphersuite",1,0xC02F', # TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256
        'AT+QSSLCFG="seclevel",1,1', # verify server certificate
        "AT+QSSLCFG=\"cacert\",1,\"#{@cacert}\"", # set CA cert
        'AT+QHTTPCFG="responseheader",1', # enable response header
        'AT+QHTTPCFG="requestheader",1', # enable request header
      ].each do |cmd|
        command!(cmd, 'OK')
        puts @log.last
      end
    end

    def post(domain, path, body, headers = {})
      url = "https://#{domain}#{path}"
      command!("AT+QIACT?", '+QIACT: 1,1,1,', nil, 10)
      command!("AT+QHTTPURL=#{url.length},80", '>')
      command!(url, 'OK', nil, 10)
      postdata = [
        "POST #{path} HTTP/1.1\r\n",
        "Host: #{domain}\r\n",
        "User-Agent: #{@user_agent}\r\n",
        "Accept: */*\r\n",
        "Content-Length: #{body.length}\r\n",
      ]
      headers.each { |k, v| postdata << "#{k}: #{v}\r\n" }
      postdata << "\r\n"
      postdata << body
      # Calculate length of postdata
      data_length = 0
      postdata.each { |line| data_length += line.length }
      command!("AT+QHTTPPOST=#{data_length},80,80", 'CONNECT')
      postdata.each do |line|
        @uart.write line
        sleep_ms 10
      end
      res = uart_read
      if res.include?("NO CARRIER") || res.include?("CME ERROR")
        raise "Post error: #{res}"
      end
      command!('AT+QHTTPREAD=80', 'OK')
      (log = @log.last) ? log[:res] : ""
    end
  end
end

