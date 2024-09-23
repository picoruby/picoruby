yaml_string = <<~YAML
name: John Doe
age: 30
address:
  street: 123 Main St
  city: Anytown
  country: USA
hobbies:
  - reading
  - cycling
  - cooking
is_student: false
gpa: 3.5
YAML

data = YAML.load(yaml_string)
puts data
