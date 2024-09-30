require 'json'
require 'mbedtls'
require 'base64'

module JWT
  def self.encode(paylaod, secret = nil, validate = 'none', algorithm: 'HS256')
    header = { 'alg' => algorithm, 'typ' => 'JWT' }
    segments = []
    segments << Base64.urlsafe_encode64(JSON.generate header)
    segments << Base64.urlsafe_encode64(JSON.generate paylaod)
    signing_input = segments.join('.')
    case validate
    when 'none'
      # no-op
    else
      raise "Not implemented"
    end
    case algorithm
    when 'HS256'
      digest = MbedTLS::Digest.new(:sha256)
      digest.update(signing_input)
      signature = digest.finish
    else
      raise "Algorithm: #{algorithm} not supported"
    end
    # @type var signature: String
    segments << Base64.urlsafe_encode64(signature)
    segments.join('.')
  end

  def self.decode(token, secret = nil, validate = 'none')
    segments = token.split('.')
    case segments.length
    when 3
      # no-op
    else
      raise "Invalid token"
    end
    header = JSON.parse(Base64.urlsafe_decode64(segments[0]))
    payload = JSON.parse(Base64.urlsafe_decode64(segments[1]))
    case validate
    when 'none'
      # no-op
    else
      raise "Not implemented"
    end
    case header['alg']
    when 'HS256'
      digest = MbedTLS::Digest.new(:sha256)
      digest.update(segments[0] + '.' + segments[1])
      signature = digest.finish
      if signature != Base64.decode64(segments[2])
        raise "Invalid signature"
      end
    else
      raise "Algorithm: #{header['alg']} not supported"
    end
    payload
  end
end
