require 'json'
require 'mbedtls'
require 'base64'

module JWT
  class VerificationError < StandardError; end

  def self.encode(paylaod, secret = nil, algorithm = 'none')
    header = { 'alg' => algorithm }
    segments = []
    segments << Base64.urlsafe_encode64(JSON.generate header)
    segments << Base64.urlsafe_encode64(JSON.generate paylaod)
    signing_input = segments.join('.')
    case algorithm.to_s.downcase
    when 'none'
      signature = ''
    when 'hs256'
      raise TypeError, "secret must be a string" unless secret.is_a? String
      hmac = MbedTLS::HMAC.new(secret, "sha256")
      hmac.update(signing_input)
      signature = hmac.digest
    when 'rs256'
      raise TypeError, "RSA private key required" unless secret.is_a? String
      begin
        private_key = MbedTLS::PKey::RSA.new(secret)
        signature = private_key.sign(MbedTLS::Digest.new(:sha256), signing_input)
      rescue => e
        raise JWT::VerificationError.new(e.message)
      end
    else
      raise "Algorithm: #{algorithm} not supported"
    end
    # @type var signature: String
    segments << Base64.urlsafe_encode64(signature)
    segments.join('.')
  end

  def self.decode(token, key = nil, validate = true)
    segments = token.split('.')
    case segments.length
    when 3
      # no-op
    else
      raise "Invalid token"
    end
    header = JSON.parse(Base64.urlsafe_decode64(segments[0]))
    payload = JSON.parse(Base64.urlsafe_decode64(segments[1]))
    if validate
      case header['alg']&.downcase
      when 'none'
        raise JWT::VerificationError.new("Signature verification failed") unless segments[2].empty?
      when 'hs256'
        raise TypeError, "key must be a string" unless key.is_a? String
        hmac = MbedTLS::HMAC.new(key, "sha256")
        hmac.update(segments[0] + '.' + segments[1])
        signature = hmac.digest
        if signature != Base64.urlsafe_decode64(segments[2])
          raise JWT::VerificationError.new("Signature verification failed")
        end
      when 'rs256'
        raise TypeError, "RSA public key required" unless key.is_a? String
        public_key = MbedTLS::PKey::RSA.new(key)
        header_payload = segments[0] + '.' + segments[1]
        decoded_signature = Base64.urlsafe_decode64(segments[2])
        unless public_key.verify(MbedTLS::Digest.new(:sha256), header_payload, decoded_signature)
          raise JWT::VerificationError.new("Signature verification failed")
        end
      else
        raise "Algorithm: #{header['alg']} not supported"
      end
    end
    [payload, header]
  end
end
