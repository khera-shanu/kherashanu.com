import { useState, useEffect } from "react"
import { useParams, useNavigate } from "react-router-dom"
import { Button } from "@/components/ui/button"
import { Input } from "@/components/ui/input"
import { MarkdownBroadcaster } from "@/components/markdown/MarkdownBroadcaster"
import { Save, FileUp, ArrowLeft, Loader2 } from "lucide-react"
import { createBlog, updateBlog, getBlogBySlug, type BlogPost } from "@/lib/api"

export default function EditorPage() {
    const { slug } = useParams<{ slug?: string }>()
    const navigate = useNavigate()

    const [title, setTitle] = useState("")
    const [urlSlug, setUrlSlug] = useState("")
    const [description, setDescription] = useState("")
    const [summary, setSummary] = useState("")
    const [category, setCategory] = useState("")
    const [tags, setTags] = useState("")
    const [content, setContent] = useState("# New Post\n\nStart writing...")
    const [saving, setSaving] = useState(false)
    const [loading, setLoading] = useState(!!slug)
    const [error, setError] = useState("")

    // Load existing post if editing
    useEffect(() => {
        if (!slug) return

        const load = async () => {
            try {
                const post = await getBlogBySlug(slug)
                setTitle(post.title)
                setUrlSlug(post.url_slug)
                setDescription(post.description || "")
                setSummary(post.summary || "")
                setCategory(post.category || "")
                setTags(post.tags || "")
                setContent(post.content || "")
            } catch (err) {
                setError("Failed to load post")
            } finally {
                setLoading(false)
            }
        }
        load()
    }, [slug])

    // Auto-generate slug from title
    const handleTitleChange = (newTitle: string) => {
        setTitle(newTitle)
        if (!slug && !urlSlug) {
            setUrlSlug(newTitle.toLowerCase()
                .replace(/[^a-z0-9\s-]/g, '')
                .replace(/\s+/g, '-')
                .replace(/-+/g, '-')
                .trim())
        }
    }

    const handleSave = async (publish: boolean) => {
        if (!title || !urlSlug) {
            setError("Title and URL slug are required")
            return
        }

        setSaving(true)
        setError("")

        const post: Partial<BlogPost> = {
            title,
            url_slug: urlSlug,
            description,
            summary,
            category,
            tags,
            content,
            status: publish ? 1 : 0
        }

        try {
            if (slug) {
                await updateBlog(slug, post)
            } else {
                await createBlog(post)
            }
            navigate('/admin/dashboard')
        } catch (err) {
            setError(err instanceof Error ? err.message : "Save failed")
        } finally {
            setSaving(false)
        }
    }

    if (loading) {
        return (
            <div className="flex items-center justify-center h-screen">
                <Loader2 className="h-8 w-8 animate-spin" />
            </div>
        )
    }

    return (
        <div className="flex flex-col h-[calc(100vh-4rem)]">
            {/* Toolbar */}
            <div className="border-b px-6 py-3 flex items-center justify-between bg-card">
                <div className="flex items-center gap-4">
                    <Button variant="ghost" size="sm" onClick={() => navigate('/admin/dashboard')}>
                        <ArrowLeft className="w-4 h-4 mr-2" />
                        Back
                    </Button>
                    <h2 className="font-semibold text-lg">{slug ? "Edit Post" : "New Post"}</h2>
                </div>
                <div className="flex gap-2">
                    <Button
                        variant="outline"
                        size="sm"
                        onClick={() => handleSave(false)}
                        disabled={saving}
                    >
                        {saving && <Loader2 className="w-4 h-4 mr-2 animate-spin" />}
                        <Save className="w-4 h-4 mr-2" />
                        Save Draft
                    </Button>
                    <Button
                        size="sm"
                        onClick={() => handleSave(true)}
                        disabled={saving}
                    >
                        {saving && <Loader2 className="w-4 h-4 mr-2 animate-spin" />}
                        <FileUp className="w-4 h-4 mr-2" />
                        Publish
                    </Button>
                </div>
            </div>

            {/* Metadata */}
            <div className="border-b px-6 py-4 bg-card/50 grid grid-cols-2 md:grid-cols-4 gap-4">
                <Input
                    placeholder="Post Title"
                    value={title}
                    onChange={(e) => handleTitleChange(e.target.value)}
                    className="font-medium"
                />
                <Input
                    placeholder="url-slug"
                    value={urlSlug}
                    onChange={(e) => setUrlSlug(e.target.value)}
                />
                <Input
                    placeholder="Description (SEO)"
                    value={description}
                    onChange={(e) => setDescription(e.target.value)}
                />
                <textarea
                    placeholder="Summary (Markdown)"
                    value={summary}
                    onChange={(e) => setSummary(e.target.value)}
                    className="flex min-h-[80px] w-full rounded-md border border-input bg-background px-3 py-2 text-sm ring-offset-background placeholder:text-muted-foreground focus-visible:outline-none focus-visible:ring-2 focus-visible:ring-ring focus-visible:ring-offset-2 disabled:cursor-not-allowed disabled:opacity-50 resize-y"
                />
                <Input
                    placeholder="Category"
                    value={category}
                    onChange={(e) => setCategory(e.target.value)}
                />
                <Input
                    placeholder="Tags (comma-separated)"
                    value={tags}
                    onChange={(e) => setTags(e.target.value)}
                />
            </div>

            {error && (
                <div className="px-6 py-2 bg-destructive/10 text-destructive text-sm">
                    {error}
                </div>
            )}

            {/* Split Screen */}
            <div className="flex-1 flex overflow-hidden">
                {/* Editor Pane */}
                <div className="w-1/2 border-r flex flex-col">
                    <textarea
                        className="w-full h-full p-6 resize-none bg-background focus:outline-none font-mono text-sm leading-relaxed"
                        value={content}
                        onChange={(e) => setContent(e.target.value)}
                        placeholder="Write your markdown content here..."
                        spellCheck={false}
                    />
                </div>

                {/* Preview Pane */}
                <div className="w-1/2 overflow-y-auto p-8 bg-muted/10">
                    <MarkdownBroadcaster content={content} />
                </div>
            </div>
        </div>
    )
}

