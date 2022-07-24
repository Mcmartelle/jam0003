#[derive(Debug, Clone)]
pub struct Program {
    pub machines: Vec<MachineDef>,
}
#[derive(Debug, Clone)]
pub struct MachineDef {
    pub name: String,
    pub body: Vec<Statement>,
    pub result: Stream,
}
#[derive(Debug, Clone)]
pub enum Statement {
    Let(Vec<String>, Stream), // let x, y = s;
    Consume(Stream),          // s;
}
// Id = input
// Five = 5

#[derive(Debug, Clone)]
pub enum Stream {
    // NOTE: just parse this as Var("input")
    // Input,                        // input
    Var(String),                                 // x
    Const(Value),                                // v
    Pipe(Box<Stream>, Box<Machine>),             // s -> m
    Zip(Vec<Stream>),                            // s₁ , .. , sₙ
    Cond(Box<Stream>, Box<Stream>, Box<Stream>), // s₁ ? s₂ : s₃
    Limit(Box<Stream>, usize),                   // s{n}

    /// Only generated during evaluation.
    /// Contains the original stream to unzip,
    /// and the index with which to project.
    Unzip(Box<Stream>, usize), // let x, y = s
}

#[derive(Debug, Clone)]
pub enum Machine {
    Var(String),
    Builtin(Builtin),
    /// Only generated during evaluation.
    Defined(Vec<Statement>, Stream),
}

#[derive(Debug, Clone, Hash, PartialEq, Eq)]
pub enum Builtin {
    Add,
    Mul,
    Mod,
    Pow,
    Sqrt,
    Gte,
    Lt,
    Eq,
    Dup2,
    Dup3,
    Print,
}

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    /// All streams are infinite.
    /// When a stream is empty is keeps returning Null.
    Null,
    Num(f64),
    Str(String),
    Bool(bool),
    Tuple(Vec<Value>),
}

impl Value {
    /// Try to transform into a tuple.
    pub fn to_tuple(self) -> Option<Vec<Value>> {
        match self {
            Value::Tuple(t) => Some(t),
            _ => None,
        }
    }

    /// Try to transform into a pair.
    pub fn to_pair(self) -> Option<(Value, Value)> {
        match self {
            Value::Tuple(mut t) if t.len() == 2 => {
                let rhs = t.pop().unwrap();
                let lhs = t.pop().unwrap();
                Some((lhs, rhs))
            }
            _ => None,
        }
    }

    /// Try to transform into a number.
    pub fn to_num(self) -> Option<f64> {
        match self {
            Value::Num(f) => Some(f),
            _ => None,
        }
    }
}
