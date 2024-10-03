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
    else
      raise "Algorithm: #{algorithm} not supported"
    end
    # @type var signature: String
    segments << Base64.urlsafe_encode64(signature)
    segments.join('.')
  end

  def self.decode(token, secret = nil, validate = true)
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
      when 'hs256'
        raise TypeError, "secret must be a string" unless secret.is_a? String
        hmac = MbedTLS::HMAC.new(secret, "sha256")
        hmac.update(segments[0] + '.' + segments[1])
        signature = hmac.digest
        if signature != Base64.urlsafe_decode64(segments[2])
          raise JWT::VerificationError.new("Signature verification failed")
        end
      else
        raise "Algorithm: #{header['alg']} not supported"
      end
    end
    [payload, header]
  end
end
