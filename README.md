# metatool
This is a very simple C++ meta introspection generation tool. It was a weekend project just to see how hard it was to do this stuff. Turns out it's not that hard.

## What does it do?
It allows you to generate meta code which enables runtime introspection of struct and enum members, allowing you:

* Automatically convert enum members to their string values for display
* Iterate the members of an enum
* Iterate the members of a struct

## Limitations
Many. The tokenizer and the parser are super simple and while it works on my code samples, it make not work on yours. 

Specifically:

* No support for C++ enum class definitions
* No support for struct definitions with member functions
* Doesn't work in plain C as it relies on function overloading (and one template currently)
* Doesn't support split output of generated code, so it works best in a "unity" build where everything is included in a single .cpp file
* Only tested on the basic examples I've run through it...

See the example below in `Usage` for what *is* supported currently.
 
# How do I use it?
## Build
The build script is just a simple shell script and it requires clang, so if you don't have that you will have to work out how to build it yourself - it's only one .cpp file so it's easy.

Run the script:

    ./build.sh
  
You will end up with a `metatool` executable in the `build/` directory.

## Usage
1. In your code add an empty macro like this:
    
       #define Introspect()
  
2. For structs or enums you want to introspect, insert the macro above them like this:

       struct v3 {
           float x, y, z;
       };

       Introspect()
       struct ExampleStruct {
          int first;
          char *second;
          double third;
          char *fourth[10];
          v3 vector;
       };
  
       Introspect()
       enum ExampleEnum {
           ExampleEnum_First,
           ExampleEnum_Second = 4,
           ExampleEnum_Third,
           ExampleEnum_Fourth
       };
   
3. Run the metatool on your source file and store the output:

       /path/to/metatool myfile.cpp > meta_generated.h

4. Include `meta_generated.h` in your source file (*after* the definitions of your structs):

       #include "meta_generated.h"
  
## Example
Given the example struct and enum in `Usage` above, you can now do these kinds of things.

* Stringify an enum value:
  
      ExampleEnum e = ExampleEnum_Third;
      const char *s = meta_getName(e);
	  printf("Stringified enum value: %s\n", s);
  
* Iterate the members of an enum:

      ExampleEnum e = ExampleEnum_Third;
      Meta_Enum *enumMeta = meta_get(e);
  
	  printf("Enum named %s, with %d members\n", enumMeta->name, enumMeta->memberCount);
  
	  Meta_EnumMember *enumMembers = meta_getMembers(enumValue);
	
      for (int i = 0; i < enumMeta->memberCount; i++) {
	      printf("    %s = %d\n", enumMembers[i].name, enumMembers[i].value);
	  }
  
* Iterate the members of a struct:

      ExampleStruct s = {};
  
	  Meta_Struct *meta = meta_get(&s);
	  Meta_StructMember *members = meta_getMembers(&s);

	  printf("Members of %s:\n", meta->name);

	  for (int i = 0; i < meta->memberCount; i++) {
		    Meta_StructMember *member = members + i;

		    if (!meta_isArray(member)) {
			      printf("    %s: ", member->name);
			      void *memberPointer = meta_getMemberPtr(blah, member);

			      switch (member->type) {
				    case Meta_Type_int:
					      printf("%d\n", *(int *)memberPointer);
					      break;

				    case Meta_Type_float:
					      printf("%f\n", *(float *)memberPointer);
					      break;

				    case Meta_Type_char:
					      if (meta_isPointer(member)) {
						        printf("%s\n", *(char **)memberPointer);
					      } 
					      break;
          
                    case Meta_Type_v3: {
                        float x = ((v3 *)memberPointer)->x;
                        float y = ((v3 *)memberPointer)->y;
                        float z = ((v3 *)memberPointer)->z;
					    printf("[ %f, %f, %f ]\n", x, y, z);
				        break;
                    }

				    default:
					      break;
			      }
		    }
	  }

**NOTE: The API is subject to change, and I'm aware that the whole `meta_getMemberPtr` and resulting pointer deferencing is ugly. Perhaps judicious template usage could make this all nicer.**

**ALSO NOTE: The Meta_Type_* enum members are generated automatically.**

# API

## Basics
These are the meta structures that are defined:

    enum Meta_StructMember_Flags {
        Meta_StructMember_Flags_None    = 0, 
        Meta_StructMember_Flags_Array   = 1, // Is the member an array
        Meta_StructMember_Flags_Pointer = 2  // Is the member a pointer
    };

    struct Meta_Struct {
       const char *name;  // A literal string which is the name of your struct
       int memberCount;   // The number of members in your struct
    };

    struct Meta_StructMember {
        const char *name; // A literal string which is the name of the struct member
        Meta_Type type;   // The type of the member, from the automatically generated type enum
        int flags;        // A bitwise combination of the Meta_StructMember_Flags above
        int arraySize;    // If this member is an array, the size of the array
        size_t offset;    // The offset of the member within your struct
    };

    struct Meta_Enum {
       const char *name;  // A literal string which is the name of your enum
       int memberCount;   // The number of members in your enum
    };

    struct Meta_EnumMember {
        const char *name; // A literal string which is the name of the enum member
        int value;        // The integer value of the enum member
    };


## Structs

    Meta_Struct *meta_get(YourStruct *s)
    
Returns a `Meta_Struct` from a pointer of your introspected struct.

    Meta_StructMember *meta_getMembers(YourStruct *s)
      
Returns an array of `Meta_StructMember`, the length of which is the `memberCount` you can get from `meta_get` above.
    
## Enums

    const char *meta_getName(YourEnum value)
   
Returns a string constant which is stringified version of your enum's member name.

    Meta_Enum *meta_get(YourEnum value)
    
Returns a `Meta_Enum` from a value of your enum.

    Meta_EnumMember *meta_getMembers(YourEnum value)
    
Returns an array of `Meta_EnumMember`, the length of which is the `memberCount` you can get from `meta_get` above.
