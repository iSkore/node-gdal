#include "../gdal_common.hpp"
#include "../gdal_rasterband.hpp"
#include "rasterband_pixels.hpp"
#include "../typed_array.hpp"

namespace node_gdal {

Persistent<FunctionTemplate> RasterBandPixels::constructor;

void RasterBandPixels::Initialize(Handle<Object> target)
{
	NanScope();

	Local<FunctionTemplate> lcons = NanNew<FunctionTemplate>(RasterBandPixels::New);
	lcons->InstanceTemplate()->SetInternalFieldCount(1);
	lcons->SetClassName(NanNew("RasterBandPixels"));

	NODE_SET_PROTOTYPE_METHOD(lcons, "toString", toString);
	NODE_SET_PROTOTYPE_METHOD(lcons, "get", get);
	NODE_SET_PROTOTYPE_METHOD(lcons, "set", set);
	NODE_SET_PROTOTYPE_METHOD(lcons, "read", read);
	NODE_SET_PROTOTYPE_METHOD(lcons, "write", write);
	NODE_SET_PROTOTYPE_METHOD(lcons, "readBlock", readBlock);
	NODE_SET_PROTOTYPE_METHOD(lcons, "writeBlock", writeBlock);

	target->Set(NanNew("RasterBandPixels"), lcons->GetFunction());

	NanAssignPersistent(constructor, lcons);
}

RasterBandPixels::RasterBandPixels()
	: ObjectWrap()
{}

RasterBandPixels::~RasterBandPixels()
{}

NAN_METHOD(RasterBandPixels::New)
{
	NanScope();

	if (!args.IsConstructCall()) {
		NanThrowError("Cannot call constructor as function, you need to use 'new' keyword");
		NanReturnUndefined();
	}
	if (args[0]->IsExternal()) {
		Local<External> ext = args[0].As<External>();
		void* ptr = ext->Value();
		RasterBandPixels *f = static_cast<RasterBandPixels *>(ptr);
		f->Wrap(args.This());
		NanReturnValue(args.This());
	} else {
		NanThrowError("Cannot create RasterBandPixels directly");
		NanReturnUndefined();
	}
}

Handle<Value> RasterBandPixels::New(Handle<Value> band_obj)
{
	NanEscapableScope();

	RasterBandPixels *wrapped = new RasterBandPixels();

	v8::Handle<v8::Value> ext = NanNew<External>(wrapped);
	v8::Handle<v8::Object> obj = NanNew(RasterBandPixels::constructor)->GetFunction()->NewInstance(1, &ext);
	obj->SetHiddenValue(NanNew("parent_"), band_obj);

	return NanEscapeScope(obj);
}

NAN_METHOD(RasterBandPixels::toString)
{
	NanScope();
	NanReturnValue(NanNew("RasterBandPixels"));
}

NAN_METHOD(RasterBandPixels::get)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y;
	double val;

	NODE_ARG_INT(0, "x", x);
	NODE_ARG_INT(1, "y", y);

	CPLErr err = band->get()->RasterIO(GF_Read, x, y, 1, 1, &val, 1, 1, GDT_Float64, 0, 0);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(NanNew<Number>(val));
}

NAN_METHOD(RasterBandPixels::set)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y;
	double val;

	NODE_ARG_INT(0, "x", x);
	NODE_ARG_INT(1, "y", y);
	NODE_ARG_DOUBLE(2, "val", val);

	CPLErr err = band->get()->RasterIO(GF_Write, x, y, 1, 1, &val, 1, 1, GDT_Float64, 0, 0);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(NanUndefined());
}

NAN_METHOD(RasterBandPixels::read)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y, w, h;
	int buffer_w, buffer_h;
	int bytes_per_pixel;
	int pixel_space, line_space;
	int size, length, min_size, min_length;
	void *data;
	Handle<Value>  array;
	Handle<Object> passed_array;
	GDALDataType type;


	NODE_ARG_INT(0, "x_offset", x);
	NODE_ARG_INT(1, "y_offset", y);
	NODE_ARG_INT(2, "x_size", w);
	NODE_ARG_INT(3, "y_size", h);

	std::string type_name = "";

	buffer_w = w;
	buffer_h = h;
	type     = band->get()->GetRasterDataType();
	NODE_ARG_INT_OPT(5, "buffer_width", buffer_w);
	NODE_ARG_INT_OPT(6, "buffer_height", buffer_h);
	NODE_ARG_OPT_STR(7, "data_type", type_name);
	if(!type_name.empty()) {
		type = GDALGetDataTypeByName(type_name.c_str());
	}

	if(args.Length() >= 5 && !args[4]->IsUndefined() && !args[4]->IsNull()) {
		NODE_ARG_OBJECT(4, "data", passed_array);
		type = TypedArray::Identify(passed_array);
		if(type == GDT_Unknown) {
			NanThrowError("Invalid array");
			NanReturnUndefined();
		}
	}

	bytes_per_pixel = GDALGetDataTypeSize(type) / 8;
	pixel_space = bytes_per_pixel;
	NODE_ARG_INT_OPT(8, "pixel_space", pixel_space);
	line_space = pixel_space * buffer_w;
	NODE_ARG_INT_OPT(9, "line_space", line_space);

	if(pixel_space < bytes_per_pixel) {
		NanThrowError("pixel_space must be greater than or equal to size of data_type");
		NanReturnUndefined();
	}
	if(line_space < pixel_space * buffer_w) {
		NanThrowError("line_space must be greater than or equal to pixel_space * buffer_w");
		NanReturnUndefined();
	}

	size       = line_space * buffer_h; //bytes
	min_size   = size - (pixel_space - bytes_per_pixel); //subtract away padding on last pixel that wont be written
	length     = (size+bytes_per_pixel-1)/bytes_per_pixel;
	min_length = (min_size+bytes_per_pixel-1)/bytes_per_pixel;

	//create array if no array was passed
	if(passed_array.IsEmpty()){
		array = TypedArray::New(type, length);
		if(array.IsEmpty() || !array->IsObject()) {
			NanReturnUndefined(); //TypedArray::New threw an error
		}
		data = TypedArray::Data(array.As<Object>());
	} else {
		array = passed_array;
		if(TypedArray::Length(passed_array) < min_length) {
 			NanThrowError("Invalid array length");
			NanReturnUndefined();
 		}
 		data = TypedArray::Data(passed_array);
	}

	CPLErr err = band->get()->RasterIO(GF_Read, x, y, w, h, data, buffer_w, buffer_h, type, pixel_space, line_space);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(array);
}

NAN_METHOD(RasterBandPixels::write)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y, w, h;
	int buffer_w, buffer_h;
	int bytes_per_pixel;
	int pixel_space, line_space;
	int size, min_size, min_length;
	void *data;
	Handle<Object> passed_array;
	GDALDataType type;

	NODE_ARG_INT(0, "x_offset", x);
	NODE_ARG_INT(1, "y_offset", y);
	NODE_ARG_INT(2, "x_size", w);
	NODE_ARG_INT(3, "y_size", h);
	NODE_ARG_OBJECT(4, "data", passed_array);

	buffer_w = w;
	buffer_h = h;
	NODE_ARG_INT_OPT(5, "buffer_width", buffer_w);
	NODE_ARG_INT_OPT(6, "buffer_height", buffer_h);

	type = TypedArray::Identify(passed_array);
	if(type == GDT_Unknown) {
		NanThrowError("Invalid array");
		NanReturnUndefined();
	}

	bytes_per_pixel = GDALGetDataTypeSize(type) / 8;
	pixel_space = bytes_per_pixel;
	NODE_ARG_INT_OPT(7, "pixel_space", pixel_space);
	line_space = pixel_space * buffer_w;
	NODE_ARG_INT_OPT(8, "line_space", line_space);

	size       = line_space * buffer_h; //bytes
	min_size   = size - (pixel_space - bytes_per_pixel); //subtract away padding on last pixel that wont be read
	min_length = (min_size+bytes_per_pixel-1)/bytes_per_pixel;

	if(pixel_space < bytes_per_pixel) {
		NanThrowError("pixel_space must be greater than or equal to size of data_type");
		NanReturnUndefined();
	}
	if(line_space < pixel_space * buffer_w) {
		NanThrowError("line_space must be greater than or equal to pixel_space * buffer_w");
		NanReturnUndefined();
	}
	if(TypedArray::Length(passed_array) < min_length) {
		NanThrowError("Invalid array length");
		NanReturnUndefined();
	}

	data = TypedArray::Data(passed_array);

	CPLErr err = band->get()->RasterIO(GF_Write, x, y, w, h, data, buffer_w, buffer_h, type, pixel_space, line_space);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(NanUndefined());
}

NAN_METHOD(RasterBandPixels::readBlock)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y, w = 0, h = 0;
	NODE_ARG_INT(0, "block_x_offset", x);
	NODE_ARG_INT(1, "block_y_offset", y);

	band->get()->GetBlockSize(&w, &h);

	GDALDataType type = band->get()->GetRasterDataType();

	Handle<Value> array;

	if(args.Length() == 3 && !args[2]->IsUndefined() && !args[2]->IsNull()) {
		Handle<Object> obj;
		NODE_ARG_OBJECT(2, "data", obj);
		if(TypedArray::Identify(obj) != type) {
			NanThrowError("Array type does not match band data type");
			NanReturnUndefined();
		}
		if(TypedArray::Length(obj) < w*h) {
 			NanThrowError("Array length must be greater than or equal to blockSize.x * blockSize.y");
			NanReturnUndefined();
 		}
 		array = obj;
	} else {
		array = TypedArray::New(type, w * h);
		if(array.IsEmpty() || !array->IsObject()) {
			NanReturnUndefined(); //TypedArray::New threw an error
		}
	}

	void* data = TypedArray::Data(array.As<Object>());

	CPLErr err = band->get()->ReadBlock(x, y, data);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(array);
}

NAN_METHOD(RasterBandPixels::writeBlock)
{
	NanScope();

	Handle<Object> parent = args.This()->GetHiddenValue(NanNew("parent_")).As<Object>();
	RasterBand *band = ObjectWrap::Unwrap<RasterBand>(parent);
	if (!band->get()) {
		NanThrowError("RasterBand object has already been destroyed");
		NanReturnUndefined();
	}

	int x, y, w = 0, h = 0;

	band->get()->GetBlockSize(&w, &h);

	NODE_ARG_INT(0, "block_x_offset", x);
	NODE_ARG_INT(1, "block_y_offset", y);

	Handle<Object> obj;
	NODE_ARG_OBJECT(2, "data", obj);

	GDALDataType type = TypedArray::Identify(obj);

	if(type == GDT_Unknown || type != band->get()->GetRasterDataType()) {
		NanThrowError("Array type does not match band data type");
		NanReturnUndefined();
	}
 	if(TypedArray::Length(obj) < w*h) {
 		NanThrowError("Array length must be greater than or equal to blockSize.x * blockSize.y");
		NanReturnUndefined();
 	}

	void* data = TypedArray::Data(obj);

	CPLErr err = band->get()->WriteBlock(x, y, data);
	if(err) {
		NODE_THROW_CPLERR(err);
		NanReturnUndefined();
	}

	NanReturnValue(NanUndefined());
}


}