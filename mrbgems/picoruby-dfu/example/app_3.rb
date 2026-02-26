puts 'Hello, this is APP 3'

def fib(n)
  return n if n <= 1
  fib(n - 1) + fib(n - 2)
end

puts "fib(10) = #{fib(10)}"

def fizzbuzz(n)
  result = []
  (1..n).each do |i|
    if i % 15 == 0
      result << "FizzBuzz"
    elsif i % 3 == 0
      result << "Fizz"
    elsif i % 5 == 0
      result << "Buzz"
    else
      result << i.to_s
    end
  end
  result
end

puts fizzbuzz(20).join(", ")

# padding line 0001: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0002: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0003: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0004: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0005: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0006: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0007: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0008: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0009: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0010: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0011: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0012: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0013: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0014: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0015: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0016: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0017: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0018: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0019: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0020: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0021: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0022: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0023: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0024: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0025: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0026: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0027: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0028: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0029: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0030: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0031: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0032: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0033: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0034: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0035: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0036: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0037: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0038: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0039: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0040: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0041: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0042: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0043: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0044: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0045: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0046: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0047: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0048: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0049: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0050: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0051: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0052: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0053: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0054: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0055: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0056: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0057: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0058: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0059: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0060: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0061: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0062: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0063: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0064: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0065: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0066: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0067: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0068: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0069: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0070: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0071: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0072: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0073: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0074: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0075: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0076: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0077: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0078: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0079: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0080: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0081: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0082: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0083: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0084: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0085: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0086: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0087: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0088: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0089: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0090: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0091: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0092: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0093: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0094: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0095: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0096: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0097: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0098: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0099: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0100: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0101: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0102: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0103: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0104: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0105: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0106: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0107: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0108: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0109: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0110: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0111: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0112: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0113: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0114: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0115: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0116: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0117: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0118: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0119: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0120: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0121: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0122: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0123: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0124: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0125: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0126: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0127: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0128: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0129: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0130: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0131: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0132: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0133: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0134: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0135: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0136: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0137: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0138: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0139: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0140: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0141: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0142: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0143: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0144: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0145: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0146: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0147: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0148: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0149: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0150: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0151: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0152: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0153: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0154: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0155: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0156: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0157: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0158: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0159: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0160: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0161: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0162: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0163: abcdefghijklmnopqrstuvwxyz 0123456789
# padding line 0164: abcdefghijklmnopqrstuvwxyz 0123456789
