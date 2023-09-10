import os
from collections import defaultdict
from pathlib import Path

import frontmatter
from flask import Flask, render_template, request
from markdown import Markdown
# from html_minifier.minify import Minifier


app = Flask(__name__)

markdowner = Markdown(extensions=["fenced_code", "codehilite"])


def get_all_blogs():
    blogs_by_year_month = defaultdict(
        lambda: defaultdict(
            lambda: defaultdict(str)
        )
    )

    all_blogs = {}

    path = Path("static/blogs")
    for md_file in path.rglob("*.md"):
        with open(md_file, "r", encoding="utf-8") as f:
            metadata, content = frontmatter.parse(f.read())
            if metadata.get("published", False) is False:
                continue
            time_to_read = len(content.split(" ")) // 200 + 1
            metadata["time_to_read"] = (
                f"{time_to_read} min read"
                if time_to_read > 1
                else f"{time_to_read} min read"
            )

            # Extract the year, month, and blog title from the path
            year = md_file.parts[-4]
            month = md_file.parts[-3]
            slug = md_file.parts[-1].replace(".md", "")

            # Insert the blog details into the nested dictionary
            blog_data = {
                "metadata": metadata,
                "content": markdowner.convert(content),
            }
            blogs_by_year_month[year][month][slug] = blog_data
            all_blogs[slug] = blog_data

    return dict(blogs_by_year_month), all_blogs


blogsByYearMonth, allBlogs = get_all_blogs()


@app.after_request
def minify_and_save(response):
    response.direct_passthrough = False
    # minified_html = Minifier(response.data.decode('utf-8', errors='replace')).minify()
    minified_html = response.data.decode('utf-8', errors='replace')

    if not os.path.exists("public"):
        os.makedirs("public")
        os.system("cp -r static public/")

    route = request.path
    if "static" in route:
        return response
    if route == "/":
        route = "index.html"

    def save_html(route, minified_html):
        with open(f"public/{route}", "w") as f:
            f.write(minified_html)

    try:
        save_html(route, minified_html)
    except FileNotFoundError:
        os.makedirs(os.path.dirname(f"public/{route}"))
        save_html(route, minified_html)

    return response


@app.route("/")
def index():
    return render_template(
            "index.html",
            pageName="Blogs",
            allBlogs=allBlogs,
            blogsByYearMonth=blogsByYearMonth
        )


@app.route("/about.html")
def about():
    return render_template("about.html", pageName="About Me")


@app.route("/community.html")
def community():
    return render_template("community.html", pageName="Community")


@app.route("/projects.html")
def projects():
    return render_template("projects.html", pageName="Projects")


@app.route("/blog/<blog_id>.html")
def blog(blog_id):
    blog_data = allBlogs.get(blog_id)
    if blog_data:
        return render_template(
            "blog.html", blog=blog_data)
    return "Blog not found", 404


if __name__ == "__main__":
    app.run(debug=True, host="0.0.0.0")
