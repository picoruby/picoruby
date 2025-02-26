require 'json'
require 'mbedtls'
require 'base64'

module JWT
  class VerificationError < StandardError; end
  class DecodeError < StandardError; end
  class IncorrectAlgorithm < StandardError; end
  class ExpiredSignature < StandardError; end

  def self.encode(payload, secret = nil, algorithm = 'none', headers = {})
    headers.each do |key, value|
      if key.is_a?(Symbol)
        headers[key.to_s] = value
        headers.delete(key)
      end
    end
    headers['alg'] = algorithm
    headers['typ'] = "JWT"
    segments = []
    segments << Base64.urlsafe_encode64(JSON.generate headers)
    segments << Base64.urlsafe_encode64(JSON.generate payload)
    signing_input = segments.join('.')
    signature = ''
    case algorithm.to_s.downcase
    when 'none'
    when 'hs256'
      raise TypeError, "secret must be a string" unless secret.is_a? String
      hmac = MbedTLS::HMAC.new(secret, "sha256")
      hmac.update(signing_input)
      signature = hmac.digest
    when 'rs256'
      raise TypeError, "RSA private key required" unless secret.is_a? MbedTLS::PKey::RSA
      begin
        signature = secret.sign(MbedTLS::Digest.new(:sha256), signing_input)
      end
    else
      raise "Algorithm: #{algorithm} not supported"
    end
    segments << Base64.urlsafe_encode64(signature)
    segments.join('.')
  end

  def self.decode(token, key = nil, validate: true, algorithm: "none", ignore_exp: false)
    decoder = Decoder.new(token, key, algorithm.to_s)

    if validate
      if !ignore_exp && decoder.payload['exp'].is_a?(Integer) && decoder.payload['exp'] < Time.now.to_i
        raise ExpiredSignature.new("Signature has expired")
      end
      case decoder.header['alg'].to_s.downcase
      when 'none'
        raise VerificationError.new("Signature verification failed") unless decoder.payload.empty?
      when 'hs256'
        decoder.verify_hmac
      when 'rs256'
        decoder.verify_rsa
      else
        raise DecodeError.new("Algorithm: #{decoder.header['alg']} not supported")
      end
    end
    [decoder.payload, decoder.header]
  end

  class Decoder
    attr_reader :header, :payload

    def initialize(token, key, algorithm)
      header_b64, payload_b64, @signature_b64 = token.split(".")
      @data = "#{header_b64}.#{payload_b64}"
      @header = JSON.parse(Base64.urlsafe_decode64(header_b64.to_s))
      unless @header["alg"] == algorithm
        raise JWT::IncorrectAlgorithm.new("Expected a different algorithm")
      end
      @payload = JSON.parse(Base64.urlsafe_decode64(payload_b64.to_s))
      if @payload['exp'].nil?
        raise JWT::DecodeError.new("Missing expiration time in header")
      end
      @key = key
    end

    def verify_hmac
      raise TypeError, "HMAC secret must be a String" unless @key.is_a? String
      # @type ivar @key: String
      hmac = MbedTLS::HMAC.new(@key, "sha256")
      hmac.update(@data)
      if @signature_b64 != Base64.urlsafe_encode64(hmac.digest)
        raise JWT::VerificationError.new("Signature verification failed")
      end
    end

    def verify_rsa
      raise TypeError.new("RSA public key required") unless @key.is_a?(MbedTLS::PKey::RSA)
      signature = base64_url_decode(@signature_b64.to_s)
      # @type ivar @key: MbedTLS::PKey::RSA
      unless @key.verify(MbedTLS::Digest.new(:sha256), signature, @data)
        raise JWT::VerificationError.new("Signature verification failed")
      end
    end

    # private

    def base64_url_decode(str)
      str += '=' * ((4 - str.size % 4) % 4)
      Base64.urlsafe_decode64(str)
    end
  end
end
