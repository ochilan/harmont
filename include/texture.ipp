template <typename Scalar, int Dim, int Options>
inline texture::ptr texture::texture_1d(const Eigen::Matrix<Scalar, Dim, 1, Options>& vector, GLenum min_filter, GLenum mag_filter, GLenum wrap_s, GLenum wrap_t) {
	GLenum scalar_type = gl_type_enum<Scalar>::value;
	GLenum internal_layout = GL_RED;
	GLenum internal_format = GL_R32F;
	parameters_t_ params = { min_filter, mag_filter, wrap_s, wrap_t };
	return std::make_shared<texture>(scalar_type, internal_layout, internal_format, width, 0, 0, vector.data(), params);
}

template <typename Scalar, int Rows, int Cols, int Options>
inline texture::ptr texture::texture_2d(const Eigen::Matrix<Scalar, Rows, Cols, Options>& matrix, GLenum min_filter, GLenum mag_filter, GLenum wrap_s, GLenum wrap_t) {
	GLenum scalar_type = gl_type_enum<Scalar>::value;
	GLenum internal_layout = GL_RED;
	GLenum internal_format = GL_R32F;
	parameters_t_ params = { min_filter, mag_filter, wrap_s, wrap_t };
	if (Options & Eigen::RowMajor) return std::make_shared<texture>(scalar_type, internal_format, width, height, 0, matrix.data(), params);
	Eigen::Matrix<Scalar, Rows, Cols, Eigen::RowMajor> copy = matrix;
	return std::make_shared<texture>(scalar_type, internal_layout, internal_format, width, height, 0, copy.data(), params);
}

template <typename Scalar, int Rows, int Cols, int Options>
inline void texture::set_data(const Eigen::Matrix<Scalar, Rows, Cols, Options>& matrix) {
	ASSERTS(dims_ < 3, "texture::set_data: Matrix setter not supported for 3D textures"+SPOT)
	if (matrix.cols() == 1) {
		ASSERTS(dims_ == 1, "texture::set_data: Setting vector as data is only allowed for 1D textures"+SPOT)
		ASSERTS(matrix.rows() == width_, "texture::set_data: Incorrect vector size"+SPOT)
		set_data(matrix.data());
	}
	ASSERTS((matrix.rows() * matrix.cols()) == size(), "texture::set_data: Incorrect matrix size"+SPOT)
	if (Options & Eigen::RowMajor) set_data(matrix.data());
	Eigen::Matrix<Scalar, Rows, Cols, Eigen::RowMajor> copy = matrix;
	set_data(copy.data());
}

template <typename Scalar, int Rows, int Cols, int Options>
inline void texture::get_data(Eigen::Matrix<Scalar, Rows, Cols, Options>& matrix) {
	ASSERTS(internal_layout_ = GL_RED, "texture::set_data: Matrix setter not supported textures with more than 1 channel"+SPOT)
	ASSERTS(matrix.cols() == width_, "texture::set_data: Requested matrix col count is bigger than textures' width"+SPOT)
	ASSERTS(matrix.rows() == height_, "texture::set_data: Requested matrix row count is bigger than textures' height"+SPOT)
    get_data(matrix.data());
    if (Options & Eigen::ColMajor) matrix.transposeInPlace();
}
