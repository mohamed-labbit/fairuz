class fairuz < Formula
  desc "Arabic-first experimental programming language"
  homepage "https://github.com/REPLACE_ME/fairuz"
  url "https://github.com/REPLACE_ME/fairuz/archive/refs/tags/v0.0.0.tar.gz"
  sha256 "REPLACE_WITH_RELEASE_TARBALL_SHA256"
  license "MIT"

  depends_on "cmake" => :build
  depends_on "libomp"
  depends_on "simdutf"

  def install
    args = std_cmake_args + [
      "-DBUILD_TESTS=OFF",
      "-DCMAKE_CXX_STANDARD=23",
    ]

    system "cmake", "-S", ".", "-B", "build", *args
    system "cmake", "--build", "build"
    system "cmake", "--install", "build"
  end

  test do
    (testpath/"smoke.txt").write <<~EOS
      ف := 20
      اذا ف + 5 < 400:
          اكتب("صحيح")
      غيره:
          اكتب("خطأ")
    EOS

    assert_match "صحيح", shell_output("#{bin}/fairuz #{testpath}/smoke.txt")
  end
end
