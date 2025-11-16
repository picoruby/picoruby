class MDNSTest < Picotest::Test
  def test_message_encode_decode
    msg = MDNS::Message.new
    msg.id = 12345
    msg.qr = 0
    msg.questions << MDNS::Question.new("test.local", MDNS::TYPE_A, MDNS::CLASS_IN)

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(12345, decoded.id)
    assert_equal(0, decoded.qr)
    assert_equal(1, decoded.questions.length)
    assert_equal("test.local", decoded.questions[0].qname)
    assert_equal(MDNS::TYPE_A, decoded.questions[0].qtype)
  end

  def test_question_encode_decode
    q = MDNS::Question.new("example.local", MDNS::TYPE_PTR, MDNS::CLASS_IN)
    encoded = q.encode

    decoded, offset = MDNS::Question.decode(encoded, 0)

    assert_equal("example.local", decoded.qname)
    assert_equal(MDNS::TYPE_PTR, decoded.qtype)
    assert_equal(MDNS::CLASS_IN, decoded.qclass)
  end

  def test_resource_record_a_type
    rr = MDNS::ResourceRecord.new(
      "device.local",
      MDNS::TYPE_A,
      MDNS::CLASS_IN,
      120,
      "192.168.1.100"
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("device.local", decoded.name)
    assert_equal(MDNS::TYPE_A, decoded.rtype)
    assert_equal(120, decoded.ttl)
    assert_equal("192.168.1.100", decoded.rdata)
  end

  def test_resource_record_ptr_type
    rr = MDNS::ResourceRecord.new(
      "_http._tcp.local",
      MDNS::TYPE_PTR,
      MDNS::CLASS_IN,
      120,
      "MyService._http._tcp.local"
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("_http._tcp.local", decoded.name)
    assert_equal(MDNS::TYPE_PTR, decoded.rtype)
    assert_equal("MyService._http._tcp.local", decoded.rdata)
  end

  def test_resource_record_txt_type_array
    rr = MDNS::ResourceRecord.new(
      "service.local",
      MDNS::TYPE_TXT,
      MDNS::CLASS_IN,
      120,
      ["key1=value1", "key2=value2"]
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("service.local", decoded.name)
    assert_equal(MDNS::TYPE_TXT, decoded.rtype)
    assert_equal(2, decoded.rdata.length)
    assert_equal("key1=value1", decoded.rdata[0])
    assert_equal("key2=value2", decoded.rdata[1])
  end

  def test_resource_record_txt_type_hash
    rr = MDNS::ResourceRecord.new(
      "service.local",
      MDNS::TYPE_TXT,
      MDNS::CLASS_IN,
      120,
      {"path" => "/", "version" => "1.0"}
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("service.local", decoded.name)
    assert_equal(MDNS::TYPE_TXT, decoded.rtype)
    assert_equal(2, decoded.rdata.length)
  end

  def test_resource_record_srv_type
    rr = MDNS::ResourceRecord.new(
      "MyService._http._tcp.local",
      MDNS::TYPE_SRV,
      MDNS::CLASS_IN,
      120,
      {
        priority: 10,
        weight: 5,
        port: 8080,
        target: "device.local"
      }
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("MyService._http._tcp.local", decoded.name)
    assert_equal(MDNS::TYPE_SRV, decoded.rtype)
    assert_equal(10, decoded.rdata[:priority])
    assert_equal(5, decoded.rdata[:weight])
    assert_equal(8080, decoded.rdata[:port])
    assert_equal("device.local", decoded.rdata[:target])
  end

  def test_message_with_multiple_questions
    msg = MDNS::Message.new
    msg.id = 100
    msg.questions << MDNS::Question.new("service1.local", MDNS::TYPE_A, MDNS::CLASS_IN)
    msg.questions << MDNS::Question.new("service2.local", MDNS::TYPE_PTR, MDNS::CLASS_IN)

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(100, decoded.id)
    assert_equal(2, decoded.questions.length)
    assert_equal("service1.local", decoded.questions[0].qname)
    assert_equal("service2.local", decoded.questions[1].qname)
  end

  def test_message_with_answers
    msg = MDNS::Message.new
    msg.id = 200
    msg.qr = 1  # Response
    msg.questions << MDNS::Question.new("test.local", MDNS::TYPE_A, MDNS::CLASS_IN)
    msg.answers << MDNS::ResourceRecord.new(
      "test.local",
      MDNS::TYPE_A,
      MDNS::CLASS_IN,
      120,
      "10.0.0.5"
    )

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(200, decoded.id)
    assert_equal(1, decoded.qr)
    assert_equal(1, decoded.questions.length)
    assert_equal(1, decoded.answers.length)
    assert_equal("test.local", decoded.answers[0].name)
    assert_equal("10.0.0.5", decoded.answers[0].rdata)
  end

  def test_message_flags
    msg = MDNS::Message.new
    msg.id = 300
    msg.qr = 1
    msg.aa = 1
    msg.opcode = 0

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(1, decoded.qr)
    assert_equal(1, decoded.aa)
    assert_equal(0, decoded.opcode)
  end

  def test_empty_message
    msg = MDNS::Message.new
    msg.id = 0

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(0, decoded.id)
    assert_equal(0, decoded.questions.length)
    assert_equal(0, decoded.answers.length)
  end

  def test_ipv4_address_encoding
    rr = MDNS::ResourceRecord.new(
      "host.local",
      MDNS::TYPE_A,
      MDNS::CLASS_IN,
      60,
      "255.255.255.255"
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal("255.255.255.255", decoded.rdata)
  end

  def test_zero_ttl
    rr = MDNS::ResourceRecord.new(
      "temp.local",
      MDNS::TYPE_A,
      MDNS::CLASS_IN,
      0,
      "1.2.3.4"
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal(0, decoded.ttl)
  end

  def test_long_domain_name
    long_name = "very.long.domain.name.with.many.labels.local"
    q = MDNS::Question.new(long_name, MDNS::TYPE_A, MDNS::CLASS_IN)

    encoded = q.encode
    decoded, offset = MDNS::Question.decode(encoded, 0)

    assert_equal(long_name, decoded.qname)
  end

  def test_message_with_all_sections
    msg = MDNS::Message.new
    msg.id = 500
    msg.qr = 1
    msg.aa = 1

    msg.questions << MDNS::Question.new("query.local", MDNS::TYPE_A, MDNS::CLASS_IN)
    msg.answers << MDNS::ResourceRecord.new("query.local", MDNS::TYPE_A, MDNS::CLASS_IN, 120, "1.1.1.1")
    msg.authorities << MDNS::ResourceRecord.new("auth.local", MDNS::TYPE_A, MDNS::CLASS_IN, 120, "2.2.2.2")
    msg.additionals << MDNS::ResourceRecord.new("add.local", MDNS::TYPE_A, MDNS::CLASS_IN, 120, "3.3.3.3")

    encoded = msg.encode
    decoded = MDNS::Message.decode(encoded)

    assert_equal(1, decoded.questions.length)
    assert_equal(1, decoded.answers.length)
    assert_equal(1, decoded.authorities.length)
    assert_equal(1, decoded.additionals.length)
  end

  def test_single_label_name
    q = MDNS::Question.new("local", MDNS::TYPE_A, MDNS::CLASS_IN)

    encoded = q.encode
    decoded, offset = MDNS::Question.decode(encoded, 0)

    assert_equal("local", decoded.qname)
  end

  def test_empty_txt_record
    rr = MDNS::ResourceRecord.new(
      "service.local",
      MDNS::TYPE_TXT,
      MDNS::CLASS_IN,
      120,
      []
    )

    encoded = rr.encode
    decoded, offset = MDNS::ResourceRecord.decode(encoded, 0)

    assert_equal(0, decoded.rdata.length)
  end
end
